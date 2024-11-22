import { execSync } from 'node:child_process';
import { glob } from 'glob';
import clangFormat from 'clang-format';

const task = process.argv.slice(2).join(' ');

void run();

async function run() {
	const clangFormatNativeBinary = clangFormat.getNativeBinary();
	const workerFiles = await glob([
		'../src/**/*.cpp',
		'../include/**/*.hpp',
		'../test/src/**/*.cpp',
		'../test/include/helpers.hpp',
		'../fuzzer/src/**/*.cpp',
		'../fuzzer/include/**/*.hpp',
	]);

	switch (task) {
		case 'lint': {
			executeCmd(
				`"${clangFormatNativeBinary}" --Werror --dry-run ${workerFiles.join(
					' '
				)}`
			);

			break;
		}

		case 'format': {
			executeCmd(
				`"${clangFormatNativeBinary}" --Werror -i ${workerFiles.join(' ')}`
			);

			break;
		}

		default: {
			logError('unknown task');

			exitWithError();
		}
	}
}

function executeCmd(command) {
	try {
		execSync(command, { stdio: ['ignore', process.stdout, process.stderr] });
	} catch (error) {
		logError('executeCmd() failed');

		exitWithError();
	}
}

// eslint-disable-next-line no-unused-vars
function logInfo(message) {
	// eslint-disable-next-line no-console
	console.log(`clang-format.mjs \x1b[36m[INFO] [${task}]\x1b[0m`, message);
}

// eslint-disable-next-line no-unused-vars
function logWarn(message) {
	// eslint-disable-next-line no-console
	console.warn(`clang-format.mjs \x1b[33m[WARN] [${task}]\x1b[0m`, message);
}

function logError(message) {
	// eslint-disable-next-line no-console
	console.error(`clang-format.mjs \x1b[31m[ERROR] [${task}]\x1b[0m`, message);
}

function exitWithError() {
	process.exit(1);
}
