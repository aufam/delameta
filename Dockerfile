FROM gcc:13.2 AS builder

RUN apt update && apt install -y cmake

WORKDIR /root/delameta

COPY cmake/ cmake/
COPY CMakeLists.txt .
COPY app/CMakeLists.txt app/
COPY test/CMakeLists.txt test/
COPY version.txt .

RUN mkdir -p app/ && echo "" > app/dummy.cpp
RUN mkdir -p src/ && echo "" > src/dummy.cpp
RUN mkdir -p test/ && echo "" > test/dummy.cpp

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23 -DDELAMETA_BUILD_APP=ON -DDELAMETA_BUILD_TEST=ON -DDELAMETA_APP_STATIC_BUILD=ON

COPY app/ app/
COPY core/linux/ core/linux/
COPY include/ include/
COPY src/ src/
COPY test/ test/

RUN cmake -B build
RUN cmake --build build
RUN ctest --test-dir build --output-on-failure

FROM alpine:3.20.3

COPY --from=builder /root/delameta/build/app/delameta /usr/bin
COPY --from=builder /root/delameta/app/README.md /usr/share/delameta/assets/README.md
COPY --from=builder /root/delameta/app/README.html /usr/share/delameta/assets/index.html

EXPOSE 5000
CMD ["delameta", "--host=0.0.0.0:5000"]
