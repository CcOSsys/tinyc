# tinyc: the Terminal's shell (bash) - prompt, history, and the easy commands
# a coloured prompt:  tinyc ~/folder $
PS1='\[\e[1;36m\]tinyc\[\e[0m\] \[\e[1;35m\]${TINYC_USB:+$TINYC_USB }\[\e[0m\]\[\e[1;34m\]\w\[\e[0m\] \[\e[1;32m\]\$\[\e[0m\] '
PROMPT_DIRTRIM=3
HISTSIZE=5000; HISTFILESIZE=20000; HISTCONTROL=ignoredups:erasedups
set +H   # a ! in what you type stays a ! (no history tricks)
shopt -s histappend checkwinsize cdspell autocd 2>/dev/null
PROMPT_COMMAND='history -a; _tinyc_usb_check'
alias ls='ls --color=auto' ll='ls -lh --color=auto' la='ls -lah --color=auto'
alias ..='cd ..' ...='cd ../..'
# the easy commands (mkf rd adf rn cpf rmt op) get the line exactly as typed, so names with spaces
# and text with ' * ; & and friends are fine: "rd my list.txt", "mkf a.txt : it's 5 * 3"
_tinyc_raw() {
    local c=$1 l re="(^|[;&|(][[:space:]]*)$1([[:space:]](.*))?\$"
    l=$(HISTTIMEFORMAT= history 1 | sed 's/^ *[0-9]* *//')
    local a=""; [[ $l =~ $re ]] && a=${BASH_REMATCH[3]}
    if [ "$c" = mkf ]; then /usr/bin/mkf --raw "$a"; else /usr/bin/tinyc-easy "$c" --raw "$a"; fi
}
for _c in mkf rd adf rn cpf rmt op; do alias $_c="_tinyc_raw $_c #"; done; unset _c
# USB sticks:  lsusb (the list: usb1, usb2...)   cdusb usb1 (go into one)   xusb (leave it)
lsusb() { if [ $# -gt 0 ]; then [ "$1" = -a ] && shift; command lsusb "$@"; else /usr/bin/tinyc-usb list; fi; }
cdusb() {
    local r n d name
    r=$(/usr/bin/tinyc-usb find "$1") || return 1
    IFS=$'\t' read -r n d name <<< "$r"
    [ -z "$TINYC_USB" ] && TINYC_USB_BACK=$PWD
    cd "$d" || return 1
    export TINYC_USB=$n TINYC_USB_DIR=$d TINYC_USB_NAME=$name
    printf '\e[1;32m✔\e[0m You'"'"'re in %s (%s). Files you make now go on that stick. xusb leaves it.\n' "$n" "$name"
    /usr/bin/tinyc-hint "cd $(printf '%q' "$d")"
}
jusb() {     # eject: leave the stick first if we're in it
    local r d
    r=$(/usr/bin/tinyc-usb find "$1") || return 1
    d=$(printf '%s' "$r" | cut -f2)
    case "$PWD/" in "$d"/*) cd "${TINYC_USB_BACK:-$HOME}" 2>/dev/null || cd ~; unset TINYC_USB TINYC_USB_DIR TINYC_USB_NAME TINYC_USB_BACK ;; esac
    /usr/bin/tinyc-easy jusb "$1"
}
xusb() {
    if [ -z "$TINYC_USB" ]; then echo "You're not in a USB stick. (cdusb usb1 goes into one.)"; return 0; fi
    local n=$TINYC_USB name=$TINYC_USB_NAME
    sync
    cd "${TINYC_USB_BACK:-$HOME}" 2>/dev/null || cd ~
    unset TINYC_USB TINYC_USB_DIR TINYC_USB_NAME TINYC_USB_BACK
    printf '\e[1;32m✔\e[0m Left %s (%s). Everything is written to it.\n' "$n" "$name"
    /usr/bin/tinyc-hint "sync, then cd somewhere else"
}
# hints: the grey "(the usual Linux way: ...)" lines under the easy commands
hints() {
    case "$1" in
        off) : > /root/.tinyc-hints-off; echo "Hints are off. (hints on brings them back)" ;;
        on)  rm -f /root/.tinyc-hints-off; echo "Hints are on: easy commands also show the usual Linux way." ;;
        *)   if [ -e /root/.tinyc-hints-off ]; then echo "Hints are off. Type hints on to turn them on."
             else echo "Hints are on. Type hints off to turn them off."; fi ;;
    esac
}
# leaving the stick with cd, or pulling it out, ends "being in" it
_tinyc_usb_check() {
    [ -n "$TINYC_USB" ] || return
    if [ ! -d "$TINYC_USB_DIR" ] || ! mountpoint -q "$TINYC_USB_DIR" 2>/dev/null; then
        printf '\e[1;31m✖\e[0m %s was pulled out.\n' "$TINYC_USB"
        cd "${TINYC_USB_BACK:-$HOME}" 2>/dev/null || cd ~
        unset TINYC_USB TINYC_USB_DIR TINYC_USB_NAME TINYC_USB_BACK
    else case "$PWD/" in "$TINYC_USB_DIR"/*) ;; *) unset TINYC_USB TINYC_USB_DIR TINYC_USB_NAME TINYC_USB_BACK ;; esac
    fi
}
_tinyc_usb_tab() { COMPREPLY=($(compgen -W "$(/usr/bin/tinyc-usb names)" -- "${COMP_WORDS[COMP_CWORD]}")); }
complete -F _tinyc_usb_tab cdusb jusb

help() {
    if [ -n "$1" ]; then builtin help "$@" 2>/dev/null || "$1" --help 2>&1 | less -F; return; fi
    local c=$'\e[1;36m' b=$'\e[1m' n=$'\e[0m'
    cat <<HELP
${c}tinyc Terminal${n} - useful commands   (help COMMAND tells more about one)

${b}Easy commands${n}  (each shows the usual Linux way in grey; hints off hides that)
  mkf hi.txt : hello!     make a file (\\n in the text starts a new line)
  rd hi.txt               read a file             op hi.txt      open it in its program
  adf hi.txt : more       add to the end          rn a.txt : b.txt   rename
  cpf hi.txt : usb1       copy (to usb1, a folder or a new name)
  rmt hi.txt              into the Trash          whereami       where am I?
  lsusb                   your USB sticks         cdusb usb1 / xusb   go into one / leave
  jusb usb1               eject a stick safely
  space                   free space              net            is the internet working?

${b}Files${n}
  ls / ll           list files (ll: with sizes and dates)
  cd FOLDER         go into a folder (cd .. goes up, cd ~ goes home)
  cp A B / mv A B   copy / move or rename          rm FILE   delete (no Trash!)
  mkdir NAME        make a folder                  tree      folders as a tree
  nano FILE         edit a text file (Ctrl+S saves, Ctrl+X quits)
  cat / less FILE   show / read a file (q quits)   find . -name '*.txt'   find files
  tar xf FILE       unpack .tar.gz ...             unzip a.zip   unpack a .zip

${b}System${n}
  top               running programs (q quits)     df -h     free space
  free -h           memory          lsusb -a       USB devices
  apk add NAME      install a program (or use Apps)    apk search NAME   find one
  poweroff / reboot

${b}Internet${n}
  ping google.com   is the internet working?       ip a      network addresses
  iwctl             Wi-Fi from the Terminal        wget URL  download a file

${b}Keys${n}
  Tab               finish a name (Tab Tab: the choices)
  Up / Down         earlier commands               Ctrl+R    search earlier commands
  Ctrl+C            stop the running program       Ctrl+L    clear the screen
HELP
}
