# Wasm Cache 
## Build & Run
```console 
cd ./envoy/examples/wasm-cache/
sudo docker compose stop proxy
rm -rf lib && sudo docker compose -f docker-compose-wasm.yaml up --remove-orphans wasm_compile
sudo docker compose up --build proxy
// ...or run in the background
sudo docker compose up --build -d proxy
```
## Test
```console
curl -s http://localhost:8000
```

## Stop containers
```console
sudo docker compose stop
// Check running containers
sudo docker compose ps
```
