#!/bin/bash

md() {
	if [ ! -d "$1" ]; then
		mkdir "$1"
	fi
}

sha256() {
	local infile="${1:?Usage: sha256 <file>}"

	# SHA2-256(<file>)= <hash>
	local pieces=($(openssl dgst -sha256 "$infile"))
	local npieces=${#pieces[*]}

	# Last piece in case <file> had spaces
	echo ${pieces[$npieces - 1]}
}

# Usage: download -c <sha256> -u <url> -o <output>
download() {
	local outfile
	local expect_hash
	local url
	local usage="Usage: download -c <sha256> -u <url> -o <outfile>"

	OPTIND=1
	while getopts "c:u:o:" opt; do
		case "$opt" in
			c)
				expect_hash="$OPTARG"
				;;
			u)
				url="$OPTARG"
				;;
			o)
				outfile="$OPTARG"
				;;
			?)
				echo "$usage"
				return 1
				;;
		esac
	done

	if [ -z "$url" -o -z "$outfile" -o -z "$expect_hash" ]; then
		echo "$usage"
		return 1
	fi

	curl -L -o "$outfile" "$url" || return 1
	local measured_hash="$(sha256 $outfile)"
	if [ "$measured_hash" != "$expect_hash" ]; then
		echo "Downloaded file '$outfile' has SHA256 of '$measured_hash', but it was expected to have SHA256 of '$expect_hash'"
		return 1
	fi

	return 0
}

untar() {
	local dir
	local file

	OPTIND=1
	while getopts "d:f:" opt; do
		case "$opt" in
			f)
				file="$OPTARG"
				;;
			d)
				dir="$OPTARG"
				;;
			?)
				echo untar: unknown option \'$opt\'
				return 1
				;;
		esac
	done

	local olddir="$PWD"
	cd "${dir:?untar: Directory not specified with -d}"

	local ec=0

	if [ -n "$file" ]; then
		tar xfz "$file" --strip-components=1
	else
		echo "untar: -f not specified"
		ec=1
	fi

	cd "$olddir"
	return $ec
}

VENDOR="$PWD/vendor"
VENDORSRC="$VENDOR/src"

md "$VENDOR"
md "$VENDORSRC"
