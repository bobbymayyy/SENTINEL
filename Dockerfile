FROM alpine:3.20 AS build
RUN apk add --no-cache build-base make
WORKDIR /src
COPY . .
RUN make clean all

FROM alpine:3.20
RUN adduser -D -H -s /sbin/nologin sentinel
COPY --from=build /src/ir-sentinel /usr/local/bin/ir-sentinel
USER sentinel
ENTRYPOINT ["/usr/local/bin/ir-sentinel"]
CMD ["--host-roots"]
