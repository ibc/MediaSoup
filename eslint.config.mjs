import eslint from '@eslint/js';
import tsEslint from 'typescript-eslint';
import jestEslint from 'eslint-plugin-jest';
import prettierRecommendedEslint from 'eslint-plugin-prettier/recommended';
import globals from 'globals';

const config = tsEslint.config(
	{
		languageOptions: {
			sourceType: 'module',
			globals: { ...globals.nodeBuiltin },
		},
		linterOptions: {
			noInlineConfig: false,
			reportUnusedDisableDirectives: 'error',
		},
	},
	eslint.configs.recommended,
	{
		rules: {
			'constructor-super': 2,
			curly: [2, 'all'],
			// Unfortunatelly `curly` does not apply to blocks in `switch` cases so
			// this is needed.
			'no-restricted-syntax': [
				2,
				{
					selector: 'SwitchCase > *.consequent[type!="BlockStatement"]',
					message: 'Switch cases without blocks are disallowed',
				},
			],
			'guard-for-in': 2,
			'newline-after-var': 2,
			'newline-before-return': 2,
			'no-alert': 2,
			'no-caller': 2,
			'no-case-declarations': 2,
			'no-catch-shadow': 2,
			'no-class-assign': 2,
			'no-console': 2,
			'no-const-assign': 2,
			'no-debugger': 2,
			'no-dupe-args': 2,
			'no-dupe-keys': 2,
			'no-duplicate-case': 2,
			'no-div-regex': 2,
			'no-empty': [2, { allowEmptyCatch: true }],
			'no-empty-pattern': 2,
			'no-eval': 2,
			'no-extend-native': 2,
			'no-ex-assign': 2,
			'no-extra-bind': 2,
			'no-extra-boolean-cast': 2,
			'no-extra-label': 2,
			'no-fallthrough': 2,
			'no-func-assign': 2,
			'no-global-assign': 2,
			'no-implicit-coercion': 2,
			'no-implicit-globals': 2,
			'no-inner-declarations': 2,
			'no-invalid-regexp': 2,
			'no-invalid-this': 2,
			'no-irregular-whitespace': 2,
			'no-lonely-if': 2,
			'no-multi-str': 2,
			'no-native-reassign': 2,
			'no-negated-in-lhs': 2,
			'no-new': 2,
			'no-new-func': 2,
			'no-new-wrappers': 2,
			'no-obj-calls': 2,
			'no-proto': 2,
			'no-prototype-builtins': 0,
			'no-redeclare': 2,
			'no-regex-spaces': 2,
			'no-restricted-imports': 2,
			'no-return-assign': 2,
			'no-self-assign': 2,
			'no-self-compare': 2,
			'no-sequences': 2,
			'no-shadow': 2,
			'no-shadow-restricted-names': 2,
			'no-sparse-arrays': 2,
			'no-this-before-super': 2,
			'no-throw-literal': 2,
			'no-undef': 2,
			'no-unmodified-loop-condition': 2,
			'no-unreachable': 2,
			'no-unused-vars': [
				2,
				{ vars: 'all', args: 'after-used', caughtErrors: 'none' },
			],
			'no-unused-private-class-members': 2,
			'no-use-before-define': 0,
			'no-useless-call': 2,
			'no-useless-computed-key': 2,
			'no-useless-concat': 2,
			'no-useless-rename': 2,
			'no-var': 2,
			'object-curly-newline': 0,
			'prefer-const': 2,
			'prefer-rest-params': 2,
			'prefer-spread': 2,
			'prefer-template': 2,
			'spaced-comment': [2, 'always'],
			strict: 2,
			'valid-typeof': 2,
			yoda: 2,
		},
	},
	// NOTE: We need to apply this only to .ts files (and not to .mjs files).
	...tsEslint.configs.recommendedTypeChecked.map(item => ({
		...item,
		files: ['node/src/**/*.ts'],
	})),
	// NOTE: We need to apply this only to .ts files (and not to .mjs files).
	...tsEslint.configs.stylisticTypeChecked.map(item => ({
		...item,
		files: ['node/src/**/*.ts'],
	})),
	{
		name: 'mediasoup .ts files',
		files: ['node/src/**/*.ts'],
		languageOptions: {
			parserOptions: {
				projectService: true,
				project: 'tsconfig.json',
			},
		},
		rules: {
			'@typescript-eslint/class-literal-property-style': [2, 'getters'],
			'@typescript-eslint/consistent-generic-constructors': [
				2,
				'type-annotation',
			],
			'@typescript-eslint/dot-notation': 0,
			'@typescript-eslint/no-unused-vars': [
				2,
				{
					vars: 'all',
					args: 'after-used',
					caughtErrors: 'none',
					ignoreRestSiblings: false,
				},
			],
			// We want to use `type` instead of `interface`.
			'@typescript-eslint/consistent-type-definitions': 0,
			// Sorry, we need many `any` usage.
			'@typescript-eslint/no-explicit-any': 0,
			'@typescript-eslint/explicit-function-return-type': 2,
			'@typescript-eslint/no-unsafe-member-access': 0,
			'@typescript-eslint/no-unsafe-assignment': 0,
			'@typescript-eslint/no-unsafe-call': 0,
			'@typescript-eslint/no-unsafe-return': 0,
			'@typescript-eslint/no-unsafe-argument': 0,
			'@typescript-eslint/consistent-indexed-object-style': 0,
			'@typescript-eslint/no-empty-function': 0,
			'@typescript-eslint/restrict-template-expressions': 0,
			'@typescript-eslint/no-duplicate-type-constituents': [
				2,
				{ ignoreUnions: true },
			],
		},
	},
	{
		name: 'mediasoup .ts test files',
		...jestEslint.configs['flat/recommended'],
		files: ['node/src/test/**/*.ts'],
		rules: {
			...jestEslint.configs['flat/recommended'].rules,
			'jest/no-disabled-tests': 2,
			'jest/prefer-expect-assertions': 0,
			'@typescript-eslint/no-unnecessary-type-assertion': 0,
		},
	},
	prettierRecommendedEslint
);

export default config;
