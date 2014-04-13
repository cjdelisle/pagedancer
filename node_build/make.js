/* vim: set expandtab ts=4 sw=4: */
/*
 * You may redistribute this program and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
var Fs = require('fs');
var nThen = require('nthen');
var Spawn = require('child_process').spawn;
var Codestyle = require('./Codestyle');
var Os = require('os');

// ['linux','darwin','sunos','win32','freebsd']
var SYSTEM = process.platform;
var CROSS = process.env['CROSS'] || '';
var GCC = process.env['CC'] || 'gcc';
var NAME = 'PageDancer';

var BUILDDIR = process.env['BUILDDIR'];
if (BUILDDIR === undefined) {
    BUILDDIR = 'build_'+SYSTEM;
}

process.on('exit', function () {
    console.log("Total build time: " + Math.floor(process.uptime() * 1000) + "ms.");
});

var Builder = require('./builder');

Builder.configure({
    rebuildIfChanges: Fs.readFileSync(__filename).toString('utf8') + JSON.stringify(process.env),
    buildDir: BUILDDIR
}, function(builder, waitFor) {

    builder.config.systemName = SYSTEM;
    builder.config.gcc = GCC;

    builder.config.tempDir = '/tmp';
    builder.config.useTempFiles = true;
    builder.config.cflags.push(
        '-std=c99',
        '-Wall',
        '-Wextra',
        '-Werror',
        '-Wno-pointer-sign',
        '-pedantic',
        '-D',builder.config.systemName + '=1',
        '-Wno-unused-parameter',
        '-Wno-unused-result',

        // Broken GCC patch makes -fstack-protector-all not work
        // workaround is to give -fno-stack-protector first.
        // see: https://bugs.launchpad.net/ubuntu/+source/gcc-4.5/+bug/691722
        '-fno-stack-protector',
        '-fstack-protector-all',
        '-Wstack-protector',

        '-g',

        '-O2'

//        '-flto' not available on some  machines
    );
    if (SYSTEM === 'linux') {
        builder.config.ldflags.push('-Wl,-z,relro,-z,now,-z,noexecstack');
    }
    if (process.env['NO_PIE'] === undefined) {
        builder.config.cflags.push('-fPIE');
        builder.config.ldflags.push('-pie');
    }

    if (/.*clang.*/.test(GCC) || SYSTEM === 'darwin') {
        // blows up when preprocessing before js preprocessor
        builder.config.cflags.push(
            '-Wno-invalid-pp-token',
            '-Wno-dollar-in-identifier-extension',
            '-Wno-newline-eof',
            '-Wno-unused-value',

            // lots of places where depending on preprocessor conditions, a statement might be
            // a case of if (1 == 1)
            '-Wno-tautological-compare'
        );
    }

    // Install any user-defined CFLAGS. Necessary if you are messing about with building cnacl
    // with NEON on the BBB
    cflags = process.env['CFLAGS'];
    if (cflags) {
        flags = cflags.split(' ');
        flags.forEach(function(flag) {
            builder.config.cflags.push(flag);
        });
    }

    // We also need to pass various architecture/floating point flags to GCC when invoked as
    // a linker.
    ldflags = process.env['LDFLAGS'];
    if (ldflags) {
        flags = ldflags.split(' ');
        flags.forEach(function(flag) {
            builder.config.ldflags.push(flag);
        });
    }

    if (/.*android.*/.test(GCC)) {
        builder.config.cflags.push(
            '-Dandroid=1'
        );
    }

}).build(function (builder, waitFor) {

    builder.buildExecutable('example_adder.c', BUILDDIR+'/example_adder', waitFor());
    builder.buildExecutable('test/PageDancer_test.c', BUILDDIR+'/PageDancer_test', waitFor());
    builder.buildExecutable('test/PageDancer_benchmark.c',
                            BUILDDIR+'/PageDancer_benchmark', waitFor());
    builder.buildExecutable('test/Benchmark_childProcesses.c',
                            BUILDDIR+'/Benchmark_childProcesses', waitFor());

}).test(function (builder, waitFor) {

    nThen(function (waitFor) {

        if (CROSS) { console.log("Cross compiling.  Test disabled."); return; }
        var out = '';
        var err = '';
        console.log("Testing:");
        var test = Spawn(BUILDDIR+'/PageDancer_test', ['all']);
        test.stdout.on('data', function(dat) { out += dat.toString(); });
        test.stderr.on('data', function(dat) { err += dat.toString(); });
        test.on('close', waitFor(function (ret) {
            if (ret !== 0) {
                console.log(out);
                console.log(err);
                console.log('\033[1;31mFailed to build '+NAME+'.\033[0m');
                waitFor.abort();
            } else {
                console.log(err);
            }
        }));

    }).nThen(function (waitFor) {

        console.log("Checking codestyle");
        var files = [];
        builder.rebuiltFiles.forEach(function (fileName) {
            if (fileName.indexOf('/dependencies/') !== -1) { return; }
            console.log("Checking " + fileName);
            files.push(fileName);
        });
        Codestyle.checkFiles(files, waitFor(function (output) {
            if (output !== '') {
                throw new Error("Codestyle failure\n" + output);
            }
        }));

    }).nThen(waitFor());

}).pack(function (builder, waitFor) {

    Fs.exists(BUILDDIR+'/example_adder', waitFor(function (exists) {
        if (!exists) { return; }
        Fs.rename(BUILDDIR+'/example_adder', './example_adder', waitFor(function (err) {
            if (err) { throw err; }
        }));
    }));

    console.log('\033[1;32mBuild completed successfully.\033[0m');

});
