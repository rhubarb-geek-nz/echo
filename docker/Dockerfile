# Copyright (c) 2025 Roger Brown.
# Licensed under the MIT License.

FROM alpine:3.21 AS builder

RUN apk update

RUN apk add alpine-sdk

COPY echod.c .

COPY config.h .

RUN gcc -I. echod.c -o /usr/bin/echod -DHAVE_CONFIG_H -Wall -Werror

FROM alpine:3.21

COPY --from=builder /usr/bin/echod /usr/bin/echod

EXPOSE 7

CMD ["/usr/bin/echod"]
