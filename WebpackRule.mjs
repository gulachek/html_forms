import { Path } from 'esmakefile';
import webpack from 'webpack';
import { dirname, basename } from 'node:path';

class WebpackRule {
	wp;
	config;
	srcPath;
	outPath;

	constructor(opts, srcPath, outPath, config) {
		if (opts.isDevelopment) {
			config.mode = 'development';
			config.devtool = 'inline-source-map';
		}

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

	recipe(args) {
		const [src, out] = args.absAll(this.srcPath, this.outPath);

		if (!this.wp) {
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
			this.wp = webpack(this.config);
		}

		return new Promise((res) => {
			this.wp.run((err, stats) => {
				const statsJson = stats.toJson();
				statsJson.modules.forEach((mod) => {
					if (mod.nameForCondition) {
						args.addPostreq(mod.nameForCondition);
					}
				});

				err && args.logStream.write(err);
				args.logStream.write(stats.toString({ colors: true }));
				res(!(err || stats.hasErrors()));

				this.wp.close((closeErr) => {
					closeErr && console.error('Error closing webpack', closeErr);
				});
			});
		});
	}
}

export function addWebpack(book, opts, srcPath, outPath, config) {
	const wp = new WebpackRule(opts, srcPath, outPath, config);
	book.add(wp);
}
