#!/bin/zsh

export CATUI_ADDRESS="test.sock"

function c() {
	./build/client
}

function serve() {
	if [[ -z "$CATUID" ]]; then
		read CATUID'?Path to catuid: '
	fi

	[[ -e "$CATUI_ADDRESS" ]] && rm "$CATUI_ADDRESS"
	"$CATUID" -s "$CATUI_ADDRESS" -p build/catui
}
