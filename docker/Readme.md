# Docker build files.

The files in this directory is used to build the docker container for the project.

## logbt

[logbt](https://github.com/mapbox/logbt) is a nice script that
provide back-traces in the containers log (console output) if
the application crash.

On Linux, it expects cores-files to be outputted to `/tmp/logbt-coredumps/core.%p.%E`.

```sh
echo "/tmp/logbt-coredumps/core.%p.%E" | sudo tee /proc/sys/kernel/core_pattern
```

Build the container-image with the `--logbt` option to enable this.


From the source-directory, enter:
```sh
./build-docker-image.sh --logbt
```

