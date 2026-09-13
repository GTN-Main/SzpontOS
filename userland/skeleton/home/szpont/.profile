# ~/.profile - User profile for szpont
export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin
export TERM=xterm-256color
export COLORTERM=truecolor
export ENV=$HOME/.shrc

if [ -f "$HOME/.shrc" ]; then
    . "$HOME/.shrc"
fi
