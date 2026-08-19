FROM alpine:3.24 AS build
RUN apk add --no-cache build-base make
WORKDIR /src
COPY . .
RUN make clean check

FROM alpine:3.24
RUN adduser -D -H -s /sbin/nologin sentinel
COPY --from=build /src/sentinel /usr/local/bin/sentinel
RUN ln -s /usr/local/bin/sentinel /usr/local/bin/ir-sentinel
USER sentinel
ENTRYPOINT ["/usr/local/bin/sentinel"]
CMD ["--host-roots"]
