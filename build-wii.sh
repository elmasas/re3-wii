#!/usr/bin/env bash
set -eu

ROOT="$(cd "$(dirname "$0")" && pwd)"

if [[ -z "${DEVKITPRO:-}" ]]; then
	for dir in /opt/devkitpro /c/devkitPro /c/msys64/opt/devkitpro; do
		if [[ -d "${dir}/devkitPPC" ]]; then
			DEVKITPRO="${dir}"
			break
		fi
	done
fi

if [[ -z "${DEVKITPRO:-}" ]]; then
	echo "devkitPro not found. Install it from https://devkitpro.org/wiki/Getting_Started"
	exit 1
fi

export DEVKITPRO
export DEVKITPPC="${DEVKITPRO}/devkitPPC"
export PATH="${DEVKITPPC}/bin:${DEVKITPRO}/tools/bin:${PATH}"

install_pkg() {
	local pacman="" cmd
	for cmd in dkp-pacman pacman; do
		if command -v "${cmd}" &>/dev/null; then
			pacman="${cmd}"
			break
		fi
	done

	if [[ -z "${pacman}" ]]; then
		echo "Install ${1} first."
		exit 1
	fi

	local sudo_cmd=""
	if [[ "${pacman}" == "dkp-pacman" && "$(id -u)" -ne 0 ]]; then
		sudo_cmd="sudo"
	fi

	echo "Installing ${1}..."
	${sudo_cmd} ${pacman} -S --needed --noconfirm "${1}"
}

if ! command -v powerpc-eabi-g++ &>/dev/null; then
	install_pkg wii-dev
fi

if [[ ! -f "${DEVKITPRO}/portlibs/ppc/lib/libmpg123.a" ]]; then
	install_pkg ppc-mpg123
fi

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

make -C "${ROOT}" -f Makefile.wii -j"${JOBS}" "$@"

echo
echo "Done: ${ROOT}/apps/re3/boot.dol"
