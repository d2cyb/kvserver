FROM ubuntu:questing AS boost-dev
RUN apt-get update
RUN apt-get install -y \
	libboost-all-dev


# build stage
FROM boost-dev AS build

RUN apt-get update
RUN apt-get install -y \
	build-essential \
	catch2 \
	clang \
	cmake \
	cppcheck \
	lcov \
	libboost-all-dev \
	ninja-build \
	valgrind

COPY . /usr/src/app
RUN rm -Rf /usr/src/app/build

WORKDIR /usr/src/app


# stage release build
FROM build AS release-build
RUN cmake -S . -B build/release --preset ci-release
RUN cmake --build build/release --target all --

# stage release run
FROM boost-dev AS release
WORKDIR /app
COPY --from=release-build /usr/src/app/build/release/bin/kvserver .
CMD ["./kvserver"]


# stage develop build
FROM build AS develop-build
RUN cmake -S . -B build/dev --preset dev-clang
RUN cmake --build build/dev --target all --

# stage develop run
FROM boost-dev AS develop
WORKDIR /app
COPY --from=release-build /usr/src/app/build/release/bin/kvserver .
CMD ["./kvserver"]


# stage run unit tests
FROM develop AS tests-unit
RUN build/dev/bin/kvserver_unit

# stage run benchmark tests
FROM develop AS tests-benchmark
RUN build/dev/bin/kvserver_bench

# stage sanitize
FROM build AS tests-sanitize
RUN cmake -S . -B build/sanitize --preset ci-sanitize
RUN cmake --build build/sanitize --target sanitize-memory

# stage test coverage
FROM build AS tests-coverage
RUN cmake -S . -B build/coverage --preset ci-coverage
RUN cmake --build build/coverage --target all --
RUN cmake --build build/coverage --target all coverage-html
