#!/bin/bash ../.port_include.sh
port=tmux
version=3.1b
files="https://github.com/tmux/tmux/releases/download/${version}/tmux-${version}.tar.gz tmux-${version}.tar.gz"

useconfigure="true"
depends="libevent ncurses"
