FROM espressif/idf:release-v5.4

USER root
RUN <<EOF
    apt-get update
    apt-get install -y \
        clang-format \
        locales \
        sudo \
        udev
    locale-gen en_US.UTF-8
    update-locale LANG=en_US.UTF-8
EOF

RUN <<EOF
    # Add user account to sudoers
    echo ubuntu ' ALL = NOPASSWD: ALL' > /etc/sudoers.d/ubuntu
    chmod 0440 /etc/sudoers.d/ubuntu
EOF

USER ubuntu

RUN echo "source /opt/esp/entrypoint.sh\nset +e" > ~/.bashrc

ENTRYPOINT ["/bin/bash", "-c"]
SHELL ["/bin/bash", "-c"]
CMD ["bin/bash"]
