set -eu

shell=${1:?expected path to shell executable}

fail()
{
    printf '%s\n' "test failed: $1" >&2
    exit 1
}

output=$(printf 'echo hello world\n' | "$shell")
[ "$output" = 'hello world' ] || fail 'external command and arguments'

output=$(printf 'echo first;echo second; echo third\n' | "$shell")
[ "$output" = "$(printf 'first\nsecond\nthird')" ] || fail 'semicolon-separated commands execute in order'

output=$(SHIT_TEST_VALUE=parent-value "$shell" <<'EOF'
echo $SHIT_TEST_VALUE
echo ${SHIT_TEST_VALUE}
EOF
)
[ "$output" = "$(printf 'parent-value\nparent-value')" ] || fail 'environment variable expansion'

output=$(SHIT_TEST_VALUE=parent-value "$shell" <<'EOF'
env
EOF
)
case "$output" in
    *'SHIT_TEST_VALUE=parent-value'*) ;;
    *) fail 'external commands inherit environment variables' ;;
esac

output=$(SHELL=/bin/sh "$shell" <<'EOF'
echo $SHELL
EOF
)
[ "$output" = '/bin/sh' ] || fail 'SHELL expansion'

output=$(PATH=/usr/bin:/bin "$shell" <<'EOF'
echo $PATH
EOF
)
[ "$output" = '/usr/bin:/bin' ] || fail 'PATH expansion and command lookup'

output=$(printf 'cd /tmp\npwd\nexit\n' | "$shell")
[ "$output" = '/tmp' ] || [ "$output" = '/private/tmp' ] || fail 'cd and pwd builtins'

output=$(HOME=/tmp "$shell" <<'EOF'
cd
pwd
EOF
)
[ "$output" = '/tmp' ] || [ "$output" = '/private/tmp' ] || fail 'cd uses HOME'

set +e
printf 'false\nexit\n' | "$shell"
status=$?
set -e
[ "$status" -eq 1 ] || fail 'exit inherits the previous status'

set +e
printf 'exit 7\n' | "$shell"
status=$?
set -e
[ "$status" -eq 7 ] || fail 'exit accepts an explicit status'

set +e
error_output=$(printf 'shit-command-that-does-not-exist\n' | "$shell" 2>&1)
status=$?
set -e
[ "$status" -eq 127 ] || fail 'missing commands return 127'
[ "$error_output" = 'shit: command not found: shit-command-that-does-not-exist' ] || fail 'missing commands report command not found'

set +e
error_output=$(printf '/shit-target-that-does-not-exist\n' | "$shell" 2>&1)
status=$?
set -e
[ "$status" -eq 127 ] || fail 'missing path targets return 127'
[ "$error_output" = '"/shit-target-that-does-not-exist": No such file or directory (os error 2)' ] || fail 'missing path targets report the target and OS error'

printf '%s\n' 'all shell tests passed'