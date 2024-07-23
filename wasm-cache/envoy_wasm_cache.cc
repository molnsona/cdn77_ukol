// NOLINT(namespace-envoy)
#include <utility>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <iostream>

#include "proxy_wasm_intrinsics.h"

struct Key {
  std::string scheme;
  std::string authority;
  std::string path;

  bool operator==(const Key &other) const
  { return (scheme == other.scheme            
            && authority == other.authority
            && path == other.path);
  }

  std::string get_url() const
  {
    return scheme + "://" + authority + path;
  }

  bool is_empty() {
    return scheme.empty() && authority.empty() && path.empty();
  }
};

struct KeyHasher
{
  std::size_t operator()(const Key& k) const
  {
    using std::size_t;
    using std::hash;
    using std::string;

    return ((hash<string>()(k.scheme)
             ^ (hash<string>()(k.authority) << 1)) >> 1)
             ^ (hash<string>()(k.path) << 1);
  }
};

struct Value {
  std::string body;
  std::string date;

  bool is_empty(){
    return body.empty() && date.empty();
  }
};

struct ValuePtr {
  size_t ind;
};

class RBCache;

class RingBuffer {
public:
  RingBuffer();
  
  ValuePtr add_header(Key k, RBCache* cache);
  void add_body(Value v);
  void remove_oldest(RBCache* cache);
  Value get_value(ValuePtr p) {return rb[p.ind].second;}

  void set_length(size_t l) {length = l; rb.resize(length);}

  // Debug print
  std::string print_cache();
private:
  std::vector<std::pair<Key, Value>> rb;
  size_t oldest;
  size_t newest;
  size_t length;
  size_t elems_count;

  bool is_full() {return elems_count == length;}
};

class RBCache {
public:
  RBCache() {rb.set_length(3);}

  void add_header(Key k);
  void add_body(Value v);
  void remove_elem(Key k);
  bool contains(const Key& k);
  std::string_view get_body(const Key& k);

  // Debug prints
  std::string print_keys();
  std::string print_cache();
private:  
  std::unordered_map<Key, ValuePtr, KeyHasher> keys;
  RingBuffer rb;
};

RingBuffer::RingBuffer():
  oldest(0),
  newest(0),
  length(0),
  elems_count(0)
{}

ValuePtr RingBuffer::add_header(Key k, RBCache* cache) 
{
  if(is_full()) 
    remove_oldest(cache);

  rb[newest % length] = std::make_pair(k,Value());
  newest = (newest + 1) % length;    
  ++elems_count;

  return {newest == 0 ? elems_count - 1: newest - 1};
}

void RingBuffer::add_body(Value v) 
{  
  size_t newest_ind = newest == 0 ? elems_count - 1: newest - 1;
  // If the newest element already has a body
  if(!rb[newest_ind].first.is_empty()
    && !rb[newest_ind].second.is_empty()) return;

  rb[newest_ind].second = v;
}

void RingBuffer::remove_oldest(RBCache* cache)
{
  cache->remove_elem(rb[oldest].first);
  rb[oldest] = std::make_pair(Key(), Value());
  oldest = (oldest + 1) % length;
  --elems_count;
}

void RBCache::add_header(Key k) 
{
  // If the element is already present in the cache.
  if(contains(k)) return;

  keys[k] = rb.add_header(k,this);
}

void RBCache::add_body(Value v) 
{
  rb.add_body(v);

  LOG_WARN("ADDDDDDDDDDDDDDDDDDDDDDDDDD BODY:\n" + print_keys() + print_cache());
}

void RBCache::remove_elem(Key k)
{
  keys.erase(k);
}

bool RBCache::contains(const Key& k)
{
  return keys.find(k) != keys.end();
}

std::string_view RBCache::get_body(const Key& k)
{  
  Value v = rb.get_value(keys[k]);
  return v.body;
}

std::string RBCache::print_keys(){
  std::string res = "Keys:\n";
  int i = 0;
  for(auto&& k:keys)
  {
    res += "[" + std::to_string(i) + "]" + " Key: " + k.first.get_url() + " ValuePtr: " + std::to_string(k.second.ind) + "\n";
    ++i;
  }
  return res;
}

std::string RBCache::print_cache(){
  return rb.print_cache();
}

std::string RingBuffer::print_cache() {
  std::string res = "Cache:\n";
  int i = 0;
  for(auto&& elem:rb)
  {
    res += "[" + std::to_string(i) + "]" + " Key: " + elem.first.get_url() + " body: " + elem.second.body + " date: " + elem.second.date + "\n";
    ++i;
  }
  return res;
}

RBCache rb_cache;

int req_counter = 0;

class ExampleRootContext : public RootContext {
public:
  explicit ExampleRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}

  bool onStart(size_t) override;
  bool onConfigure(size_t) override;
  void onTick() override;  
};

class ExampleContext : public Context {
public:
  explicit ExampleContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override;
  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override;
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
  FilterHeadersStatus onResponseHeaders(uint32_t headers, bool end_of_stream) override;
  FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream) override;
  void onDone() override;
  void onLog() override;
  void onDelete() override;
};
static RegisterContextFactory register_ExampleContext(CONTEXT_FACTORY(ExampleContext),
                                                      ROOT_FACTORY(ExampleRootContext),
                                                      "my_root_id");

bool ExampleRootContext::onStart(size_t) {
  LOG_TRACE("onStart");
  return true;
}

bool ExampleRootContext::onConfigure(size_t) {
  LOG_TRACE("onConfigure");
  proxy_set_tick_period_milliseconds(1000); // 1 sec
  return true;
}

void ExampleRootContext::onTick() { LOG_TRACE("onTick"); }

void ExampleContext::onCreate() { LOG_WARN(std::string("onCreate " + std::to_string(id()))); }

FilterHeadersStatus ExampleContext::onRequestHeaders(uint32_t, bool) {
  LOG_DEBUG(std::string("onRequestHeaders ") + std::to_string(id()));
  auto result = getRequestHeaderPairs();
  auto pairs = result->pairs();
  LOG_INFO(std::string("headers: ") + std::to_string(pairs.size()));
  Key key;
  for (auto& p : pairs) {
    if(p.first == ":scheme") key.scheme = p.second;
    if(p.first == ":authority") key.authority = p.second;
    if(p.first == ":path") key.path = p.second;

    LOG_INFO(std::string(p.first) + std::string(" -> ") + std::string(p.second));
  }

  ++req_counter;

  if(rb_cache.contains(key)){
    std::string_view response_body = rb_cache.get_body(key);
    HeaderStringPairs additional_headers = {};
    sendLocalResponse(200, "Success", response_body, additional_headers);
    return FilterHeadersStatus::StopAllIterationAndWatermark;
  }
  else {
    rb_cache.add_header(key);
  }

  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus ExampleContext::onResponseHeaders(uint32_t, bool) {
  LOG_DEBUG(std::string("onResponseHeaders ") + std::to_string(id()));
  auto result = getResponseHeaderPairs();
  auto pairs = result->pairs();
  LOG_INFO(std::string("headers: ") + std::to_string(pairs.size()));
  for (auto& p : pairs) {
    LOG_INFO(std::string(p.first) + std::string(" -> ") + std::string(p.second));
  }
  return FilterHeadersStatus::Continue;
}

FilterDataStatus ExampleContext::onRequestBody(size_t body_buffer_length,
                                               bool /* end_of_stream */) {
  auto body = getBufferBytes(WasmBufferType::HttpRequestBody, 0, body_buffer_length);
  LOG_ERROR(std::string("onRequestBody ") + std::string(body->view()));
  return FilterDataStatus::Continue;
}

FilterDataStatus ExampleContext::onResponseBody(size_t body_buffer_length,
                                                bool /* end_of_stream */) {
  Value val;
  val.body = getBufferBytes(WasmBufferType::HttpResponseBody, 0, body_buffer_length)->toString() + "Hello from cache " + std::to_string(req_counter) + "\n";
  LOG_WARN("Body: " + val.body);
  rb_cache.add_body(val);
  return FilterDataStatus::Continue;
}

void ExampleContext::onDone() { LOG_WARN(std::string("onDone " + std::to_string(id()))); }

void ExampleContext::onLog() { LOG_WARN(std::string("onLog " + std::to_string(id()))); }

void ExampleContext::onDelete() { LOG_WARN(std::string("onDelete " + std::to_string(id()))); }
