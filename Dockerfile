FROM espressif/idf:release-v5.4

USER root
RUN apt-get update && apt-get install -y locales && locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8

USER 1000