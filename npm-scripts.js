const process = require('process');
const os = require('os');
const fs = require('fs');
const { execSync } = require('child_process');
const { version } = require('./package.json');

const isFreeBSD = os.platform() === 'freebsd';
const isWindows = os.platform() === 'win32';
const task = process.argv.slice(2).join(' ');

// mediasoup mayor version.
const MAYOR_VERSION = version.split('.')[0];

// make command to use.
const MAKE = process.env.MAKE || (isFreeBSD ? 'gmake' : 'make');

// eslint-disable-next-line no-console
console.log(`npm-scripts.js [INFO] running task: ${task}`);

switch (task)
{
	// As per NPM documentation (https://docs.npmjs.com/cli/v9/using-npm/scripts):
	//
	// If a package being installed through git contains a prepare script, its
	// dependencies and devDependencies will be installed, and the prepare script
	// will be run, before the package is packaged and installed.
	//
	// So here we compile TypeScript and flatbuffers to JavaScript.
	case 'prepare':
	{
		if (!fs.existsSync('node/lib'))
		{
			execute('node npm-scripts.js typescript:build');
			// TODO: Compile flatbuffers.
		}

		break;
	}

	case 'postinstall':
	{
		if (!process.env.MEDIASOUP_WORKER_BIN)
		{
			execute('node npm-scripts.js worker:build');

			if (!process.env.MEDIASOUP_LOCAL_DEV)
			{
				// Clean build artifacts except `mediasoup-worker`.
				execute(`${MAKE} clean-build -C worker`);
				// Clean downloaded dependencies.
				execute(`${MAKE} clean-subprojects -C worker`);
				// Clean PIP/Meson/Ninja.
				execute(`${MAKE} clean-pip -C worker`);
			}
		}

		break;
	}

	case 'typescript:build':
	{
		if (!isWindows)
		{
			execute('rm -rf node/lib');
		}
		else
		{
			execute('rmdir /s /q "node/lib"', /* exitOnError */ false);
		}

		execute('tsc --project node');
		taskReplaceVersion();

		break;
	}

	case 'typescript:watch':
	{
		const TscWatchClient = require('tsc-watch/client');

		if (!isWindows)
		{
			execute('rm -rf node/lib');
		}
		else
		{
			execute('rmdir /s /q "node/lib"', /* exitOnError */ false);
		}

		const watch = new TscWatchClient();

		watch.on('success', taskReplaceVersion);
		watch.start('--project', 'node', '--pretty');

		break;
	}

	case 'worker:build':
	{
		execute(`${MAKE} -C worker`);

		break;
	}

	case 'lint:node':
	{
		execute('eslint -c node/.eslintrc.js --max-warnings 0 node/src/ node/.eslintrc.js npm-scripts.js node/tests/ worker/scripts/gulpfile.js');

		break;
	}

	case 'lint:worker':
	{
		execute(`${MAKE} lint -C worker`);

		break;
	}

	case 'format:worker':
	{
		execute(`${MAKE} format -C worker`);

		break;
	}

	case 'test:node':
	{
		if (!fs.existsSync('node/lib'))
		{
			execute('node npm-scripts.js typescript:build');
		}

		taskReplaceVersion();

		if (!process.env.TEST_FILE)
		{
			execute('jest');
		}
		else
		{
			execute(`jest --testPathPattern ${process.env.TEST_FILE}`);
		}

		break;
	}

	case 'test:worker':
	{
		execute(`${MAKE} test -C worker`);

		break;
	}

	case 'coverage':
	{
		taskReplaceVersion();
		execute('jest --coverage');
		execute('open-cli coverage/lcov-report/index.html');

		break;
	}

	case 'release':
	{
		execute('node npm-scripts.js typescript:build');
		execute('npm run lint');
		execute('npm run test');
		execute(`git commit -am '${version}'`);
		execute(`git tag -a ${version} -m '${version}'`);
		execute(`git push origin v${MAYOR_VERSION} && git push origin --tags`);
		execute('npm publish');

		break;
	}

	case 'install-clang-tools':
	{
		execute('npm ci --prefix worker/scripts');

		break;
	}

	default:
	{
		throw new TypeError(`unknown task "${task}"`);
	}
}

function taskReplaceVersion()
{
	// eslint-disable-next-line no-console
	console.log('npm-scripts.js [INFO] taskReplaceVersion()');

	const files =
	[
		'node/lib/index.js',
		'node/lib/index.d.ts',
		'node/lib/Worker.js'
	];

	for (const file of files)
	{
		const text = fs.readFileSync(file, { encoding: 'utf8' });
		const result = text.replace(/__MEDIASOUP_VERSION__/g, version);

		fs.writeFileSync(file, result, { encoding: 'utf8' });
	}
}

function execute(command, exitOnError = true)
{
	// eslint-disable-next-line no-console
	console.log(`npm-scripts.js [INFO] execute(): ${command}`);

	try
	{
		execSync(command, { stdio: [ 'ignore', process.stdout, process.stderr ] });
	}
	catch (error)
	{
		if (exitOnError)
		{
			process.exit(1);
		}
	}
}
