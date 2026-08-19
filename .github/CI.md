# SENTINEL CI

GitHub Actions validates SENTINEL on pull requests targeting `latest`, on pushes to `latest`, and on manual dispatch.

## Jobs

- `build-test (gcc)` runs `make clean check` with GCC.
- `build-test (clang)` runs `make clean check` with Clang.
- `static-analysis` runs Cppcheck 2.21.0 from the explicit `neszt/cppcheck-docker:2.21.0` image tag. It does not use APT at runtime.
- `container-build` validates the Dockerfile, builds the production image, and smoke-tests `sentinel --version`.

Feature-branch pushes do not independently trigger CI. Opening or updating a pull request targeting `latest` runs the validation suite once. After merge, the push to `latest` runs the same suite against the exact merged state.

Concurrency cancellation stops stale runs when a newer commit supersedes the same pull request or branch run. Checkout credentials are not persisted after the checkout step.

## Maintenance

Keep the Cppcheck image version explicit in `ci.yml`. Upgrade it deliberately in its own pull request so a static-analysis behavior change is reviewable instead of arriving through a floating `latest` tag. For stronger supply-chain immutability, pin the image by digest when the release pipeline is introduced.
