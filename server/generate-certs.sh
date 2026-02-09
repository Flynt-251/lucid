if ! command -v openssl >/dev/null 2>&1
then
    echo "openssl is needed to generate key and certificate."
    exit 1
fi

openssl genrsa -out server.key 2048
openssl req -days 365 -out server.pem -new -x509 -key server.key -subj "/C=GB/ST=West Midlands/L=Coventry/O=Some Organisation/OU=Project Lucid Development Team/CN=Some Holder"
