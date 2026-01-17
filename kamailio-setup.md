docker rm -f kamailio 2>/dev/null

docker run -d --name kamailio \
  --platform linux/amd64 \
  -p 5061:5061/udp \
  -p 5061:5061/tcp \
  -v "$PWD/kamailio/kamailio.cfg:/etc/kamailio/kamailio.cfg:ro" \
  ghcr.io/kamailio/kamailio:6.0.4-bookworm