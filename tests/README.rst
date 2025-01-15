Running the Test Suite
######################

The test suite uses Zephyr's
`Twister <https://docs.zephyrproject.org/latest/develop/test/twister.html>`_
test tool for in-device testing.

This works by building a firmware that prints that prints the test results on the console,
which is read by the twister script to generate a report.

This permits testing many tests in an automated way.

This will be integrated into a Github CI environment to trigger tests from remote.
