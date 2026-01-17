py -m venv .venv
.venv/Scripts/activate
py -m pip install -r requirements.txt

py ./grpc_server/server.py