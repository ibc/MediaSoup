# Contributing to mediasoup

Thanks for taking the time to contribute to mediasoup! 🎉👍

## License

By contributing to mediasoup, you agree that your contributions will be licensed under its ISC License.

## Reporting Bugs

We primarily use GitHub as an issue tracker. Just open an issue in GitHub if you have encountered a bug in mediasoup.

If you have questions or doubts about mediasoup or need support, please use the mediasoup Discourse Group instead:

- https://mediasoup.discourse.group

If you got a crash in mediasoup, please try to provide a core dump into the issue report:

- https://mediasoup.org/support/#crashes-in-mediasoup-get-a-core-dump

## Pull Request Process

When creating a Pull Request for mediasoup:

- Ensure that changes/additions done in TypeScript files (in `node/src` folder) are also applied to the Rust layer (in `rust` folder), and vice-versa.
- Test units must be added for both Node.js and Rust.
- Changes/additions in C++ code may need tests in `worker/test` folder.

Once all changes are done, run the following commands to verify that the code in your PR conforms to the code syntax of the project, it does not break existing funtionality and tests pass:

- `npm run lint`: Check TypeScript and C++ linting rules. Formating errors can be automatically fixed by running `npm run format`.
- `npm run typescript:build`: Compile TypeScript code (under `src` folder) into JavaScript code (under `lib` folder).
- `npm run test`: Run JavaScript and C++ test units.
- Instead, you can run `npm run release:check` which will run all those steps.
- `cargo fmt`, `cargo clippy` and `cargo test` to ensure that everything is good in Rust side.

The full list of `npm` scripts, `invoke` tasks and `cargo` commands is available in the [doc/Building.md](/doc/Building.md) file.

## Coding Style

In adition to automatic checks performed by commands above, we also enforce other minor things related to coding style:

### Comments in TypeScript and C++

We use `//` for inline comments in both JavaScript and C++ source files.

- Comments must start with upercase letter.
- Comments must not exceed 80 columns (split into different lines if necessary).
- Comments must end with a dot.

Example (good):

```ts
// Calculate foo based on bar value.
const foo = bar / 2;
```

Example (bad):

```ts
// calculate foo based on bar value
const foo = bar / 2;
```

When adding inline documentation for methods or functions, we use `/** */` syntax. Example:

```ts
/**
 * Calculates current score for foo and bar.
 */
function calculateScore(): number {
	// [...]
}
```
