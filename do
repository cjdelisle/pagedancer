#!/usr/bin/env bash
# You may redistribute this program and/or modify it under the terms of
# the GNU General Public License as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

CFLAGS="-I. -fPIE -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -g -pedantic"
LDFLAGS="-g -pie -Wl,-z,relro,-z,now,-z,noexecstack"

cc() {
    gcc -o ./build/$1.o ${CFLAGS} -c ./$1 || exit 0
}

ccl() {
    cc $1
    LINKS="${LINKS} ./build/$1.o"
}

cce() {
    cc $1
    gcc -o $2 ${LDFLAGS} ${LINKS} "./build/$1.o" || exit 0
}

[ -d build ] && rm -rf ./build
mkdir build || exit 0
mkdir build/test || exit 0

ccl Syscall.S
ccl PageDancer.c
ccl SeccompRules.c
cce adder.c adder
cce test/PageDancer_test.c PageDancer_test
