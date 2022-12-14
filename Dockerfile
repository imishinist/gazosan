FROM rhikimochi/opencv-docker:v0.14 as builder

LABEL org.opencontainers.image.source=https://github.com/imishinist/gazosan


COPY . /app
WORKDIR /app
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j $(nproc) && \
    cp gazosan ..

FROM ubuntu:rolling
WORKDIR /app
COPY --from=builder /app/build/gazosan /usr/local/bin
COPY --from=builder /usr/local/lib/ /usr/local/lib/
COPY --from=builder /lib/x86_64-linux-gnu/libjpeg.so.8 \
     /lib/x86_64-linux-gnu/libpng16.so.16 \
     /lib/x86_64-linux-gnu/libtiff.so.5 \
     /lib/x86_64-linux-gnu/libtbb.so.2 \
     /lib/x86_64-linux-gnu/libwebp.so.6 \
     /lib/x86_64-linux-gnu/libjbig.so.0 \
     /lib/x86_64-linux-gnu/
