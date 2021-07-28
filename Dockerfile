FROM alpine:3 AS build
RUN apk add alpine-sdk openssl-dev

WORKDIR /build
COPY . .

RUN make default

FROM alpine:3
COPY --from=build /build/geminon /usr/bin/geminon
COPY docroot /docroot

ENV GEMINON_CERTIFICATE=/certs/cert.pem
ENV GEMINON_PRIVATE_KEY=/certs/key.pem
ENTRYPOINT ["/usr/bin/geminon"]
EXPOSE 1965
CMD ["--static", "/:/docroot"]
