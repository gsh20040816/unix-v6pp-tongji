@setlocal
@pushd .

@call src
@if defined OOS_MAKE_JOBS (
	@echo [oos] make all -j%OOS_MAKE_JOBS%
	@make -j%OOS_MAKE_JOBS% all
) else (
	@make all
)

@popd
@endlocal