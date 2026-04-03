@setlocal
@pushd .

@call src
@if defined OOS_MAKE_JOBS (
	@echo [oos] make build -j%OOS_MAKE_JOBS%
	@make -j%OOS_MAKE_JOBS% build
) else (
	@make build
)

@popd
@endlocal