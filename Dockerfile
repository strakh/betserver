FROM gcc:8 AS build
COPY betserver /usr/src/betserver
WORKDIR /usr/src/betserver
RUN gcc --static -o betserver main.c

FROM alpine
COPY --from=build /usr/src/betserver/betserver /usr/bin/betserver
ENTRYPOINT ["/usr/bin/betserver"]
