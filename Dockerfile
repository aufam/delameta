FROM alpine:3.20.3 AS builder

RUN apk add --no-cache \
    git \
    cmake \
    make \
    g++

WORKDIR /root/delameta

COPY cmake/ cmake/
COPY CMakeLists.txt .
COPY app/CMakeLists.txt app/
COPY test/CMakeLists.txt test/

RUN mkdir -p app/ && echo "" > app/dummy.cpp && \
    mkdir -p src/ && echo "" > src/dummy.cpp && \
    mkdir -p test/ && echo "" > test/dummy.cpp && \
    cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=23 \
    -DDELAMETA_BUILD_APP=ON \
    -DDELAMETA_BUILD_TEST=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-static" && \
    cmake --build build -t gtest_main && \
    cmake --build build -t gtest

COPY core/linux/ core/linux/
COPY include/ include/
COPY src/ src/

RUN cmake -B build && \
    cmake --build build -t delameta

COPY app/ app/
COPY test/ test/

RUN cmake -B build && \
    cmake --build build -t main && \
    cmake --build build -t test_all && \
    ctest --test-dir build --output-on-failure

FROM alpine:3.20.3

COPY --from=builder /root/delameta/build/app/delameta /usr/bin
COPY app/README.md /usr/share/delameta/assets/README.md
COPY app/README.html /usr/share/delameta/assets/index.html

EXPOSE 5000
CMD ["delameta", "--host=0.0.0.0:5000"]
