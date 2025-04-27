import { cli } from 'esmakefile';
import { Distribution } from 'esmakefile-cmake';

cli((make) => {
	const d = new Distribution(make, {
		name: 'test_html_forms',
		version: '1.2.3',
		cStd: 17,
	});

	const htmlForms = d.findPackage('html_forms');

	const test = d.addTest({
		name: 'test_case',
		src: ['test.c'],
		linkTo: [htmlForms],
	});

	make.add('test', [test.run]);
});
