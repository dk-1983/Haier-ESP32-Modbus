# Security

This project is intended for a trusted local network. Modbus TCP has no authentication or TLS; do not expose port502 to the Internet. Setup credentials and OTA/web password belong only in untracked secrets.yaml. Web Basic authentication uses plain HTTP; use network isolation as appropriate. RTU/TCP are disabled by default and can be enabled independently. No factory keys or firmware are distributed.

For reports, provide a minimal reproduction with credentials, MAC addresses and personal network details removed. Before making an exploitable vulnerability public, coordinate privately with the repository maintainer through their listed contact method.
