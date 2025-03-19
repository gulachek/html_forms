import { Path } from 'esmakefile';
import webpack from 'webpack';
import { dirname, basename } from 'node:path';
import { Worker, isMainThread, parentPort } from 'node:worker_threads';

if (!isMainThread) {
	let wp;
	parentPort.on('message', (msg) => {
		if (msg.type !== 'start')
			throw new Error(`unknown webpack worker message type "${msg.type}`);

		if (!wp) {
			wp = webpack(msg.config);
		}

		wp.run((err, stats) => {
			const statsJson = stats.toJson();
			const statsColors = stats.toString({ colors: true });

			wp.close((closeErr) => {
				parentPort.postMessage({
					type: 'complete',
					err,
					statsJson,
					statsColors,
					statsHasErrors: stats.hasErrors(),
				});

				closeErr && console.error('Error closing webpack', closeErr);
			});
		});
	});
}

class WebpackRule {
	worker;
	wp;
	config;
	srcPath;
	outPath;
	workResolve;
	workReject;

	constructor(opts, srcPath, outPath, config) {
		this.worker = new Worker(new URL(import.meta.url));
		this.worker.on('message', (msg) => this.onWorkerMessage(msg));
		this.worker.unref();

		Object.assign(config, opts);
		this.config = config;
		this.srcPath = Path.src(srcPath);
		this.outPath = Path.build(outPath);
	}

	prereqs() {
		return [this.srcPath];
	}

	targets() {
		return [this.outPath];
	}

	async recipe(args) {
		const [src, out] = args.absAll(this.srcPath, this.outPath);

		if (!this.config.entry) {
			const config = this.config;
			config.entry = src;
			config.output = config.output || {};
			config.output.path = dirname(out);
			config.output.filename = basename(out);
			config.module = {
				rules: [
					{
						test: /\.ts$/,
						use: 'ts-loader',
						exclude: /node_modules/,
					},
				],
			};

			config.resolve = {
				extensions: ['.ts', '.js'],
			};
		}

		const { err, statsJson, statsColors, statsHasErrors } =
			await this.runWorker();

		statsJson.modules.forEach((mod) => {
			if (mod.nameForCondition) {
				args.addPostreq(mod.nameForCondition);
			}
		});

		err && args.logStream.write(err);
		args.logStream.write(statsColors);
		return !(err || statsHasErrors);
	}

	clearWorker() {
		delete this.workResolve;
		delete this.workReject;
	}

	runWorker() {
		if (this.workResolve) throw new Error('worker already in progress');

		return new Promise((res, rej) => {
			this.workResolve = res;
			this.workReject = rej;

			this.worker.postMessage({
				type: 'start',
				config: this.config,
			});
		});
	}

	onWorkerMessage(msg) {
		if (!this.workResolve)
			throw new Error(
				`Wasn't listenening for webpack worker message but received one`,
			);

		if (msg.type !== 'complete') {
			throw new Error(`Unexpected webpack worker message type "${type}"`);
		}

		this.workResolve(msg);
		this.clearWorker();
	}
}

export function addWebpack(make, opts, srcPath, outPath, config) {
	const wp = new WebpackRule(opts, srcPath, outPath, config);
	make.add(wp);
}
