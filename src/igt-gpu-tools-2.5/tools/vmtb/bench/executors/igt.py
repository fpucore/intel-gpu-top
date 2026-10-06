# SPDX-License-Identifier: MIT
# Copyright © 2024-2026 Intel Corporation

import enum
import json
import logging
import posixpath
import signal
import typing

from bench.executors.executor_interface import ExecutorInterface
from bench.executors.shell import ShellExecutor
from bench.machines.machine_interface import (DEFAULT_TIMEOUT,
                                              MachineInterface, ProcessResult)

logger = logging.getLogger('IgtExecutor')


class IgtType(enum.Enum):
    # Basic/generic IGT tests:
    EXEC_BASIC = enum.auto()
    EXEC_STORE = enum.auto()
    SPIN_BATCH = enum.auto()
    # VF migration workloads - xe_exec_reset/long_spin subtests:
    EXEC_RESET_LONG_SPIN_MANY_PREEMPT = enum.auto()
    EXEC_RESET_LONG_SPIN_MANY_PREEMPT_MEDIA = enum.auto()
    EXEC_RESET_LONG_SPIN_MANY_PREEMPT_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT0_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT1_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT = enum.auto()
    EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_MEDIA = enum.auto()
    EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT0_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT1_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_SYS_REUSE_MANY_PREEMPT_THREADS = enum.auto()
    EXEC_RESET_LONG_SPIN_COMP_REUSE_MANY_PREEMPT_THREADS = enum.auto()
    # VF migration workloads - xe_exec_reset/cancel subtests:
    EXEC_RESET_CANCEL = enum.auto()
    EXEC_RESET_CANCEL_PREEMPT = enum.auto()
    EXEC_RESET_CANCEL_TIMESLICE_PREEMPT = enum.auto()
    EXEC_RESET_CANCEL_TIMESLICE_MANY_PREEMPT = enum.auto()
    # VF migration workloads - xe_exec_threads subtests:
    EXEC_THREADS_BASIC = enum.auto()
    EXEC_THREADS_BAL_BASIC = enum.auto()
    EXEC_THREADS_CM_USERPTR_INVALIDATE = enum.auto()
    EXEC_THREADS_BAL_MIXED_USERPTR_INVALIDATE = enum.auto()
    EXEC_THREADS_MANY_QUEUES = enum.auto()
    # VF migration workloads - xe_ccs subtest:
    CCS_BLOCK_COPY_COMPRESSED = enum.auto()
    # VF migration workloads - xe_compute_preempt subtest:
    COMPUTE_PREEMPT_MANY = enum.auto()


# Mappings of driver specific (i915/xe) IGT instances:
# {IGT type: (i915 IGT name, xe IGT name)}
igt_tests: typing.Dict[IgtType, typing.Tuple[str, str]] = {
    # Basic/generic IGT tests:
    IgtType.EXEC_BASIC:
      ('igt@gem_exec_basic@basic', 'igt@xe_exec_basic@once-basic'),
    IgtType.EXEC_STORE:
      ('igt@gem_exec_store@dword', 'igt@xe_exec_store@basic-store'),
    IgtType.SPIN_BATCH:
      ('igt@gem_spin_batch@legacy', 'igt@xe_spin_batch@spin-basic'),
    # VF migration workloads - xe_exec_reset/long_spin subtests:
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT:
      ('n/a', 'igt@xe_exec_reset@long-spin-many-preempt'),
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_MEDIA:
      ('n/a', 'igt@xe_exec_reset@long-spin-many-preempt-media'),
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-many-preempt-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT0_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-many-preempt-gt0-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT1_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-many-preempt-gt1-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT:
      ('n/a', 'igt@xe_exec_reset@long-spin-reuse-many-preempt'),
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_MEDIA:
      ('n/a', 'igt@xe_exec_reset@long-spin-reuse-many-preempt-media'),
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-reuse-many-preempt-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT0_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-reuse-many-preempt-gt0-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT1_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-reuse-many-preempt-gt1-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_SYS_REUSE_MANY_PREEMPT_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-sys-reuse-many-preempt-threads'),
    IgtType.EXEC_RESET_LONG_SPIN_COMP_REUSE_MANY_PREEMPT_THREADS:
      ('n/a', 'igt@xe_exec_reset@long-spin-comp-reuse-many-preempt-threads'),
    # VF migration workloads - xe_exec_reset/cancel subtests:
    IgtType.EXEC_RESET_CANCEL:
      ('n/a', 'igt@xe_exec_reset@cancel'),
    IgtType.EXEC_RESET_CANCEL_PREEMPT:
      ('n/a', 'igt@xe_exec_reset@cancel-preempt'),
    IgtType.EXEC_RESET_CANCEL_TIMESLICE_PREEMPT:
      ('n/a', 'igt@xe_exec_reset@cancel-timeslice-preempt'),
    IgtType.EXEC_RESET_CANCEL_TIMESLICE_MANY_PREEMPT:
      ('n/a', 'igt@xe_exec_reset@cancel-timeslice-many-preempt'),
    # VF migration workloads - xe_exec_threads subtests:
    IgtType.EXEC_THREADS_BASIC:
      ('n/a', 'igt@xe_exec_threads@threads-basic'),
    IgtType.EXEC_THREADS_BAL_BASIC:
      ('n/a', 'igt@xe_exec_threads@threads-bal-basic'),
    IgtType.EXEC_THREADS_CM_USERPTR_INVALIDATE:
      ('n/a', 'igt@xe_exec_threads@threads-cm-userptr-invalidate'),
    IgtType.EXEC_THREADS_BAL_MIXED_USERPTR_INVALIDATE:
      ('n/a', 'igt@xe_exec_threads@threads-bal-mixed-userptr-invalidate'),
    IgtType.EXEC_THREADS_MANY_QUEUES:
      ('n/a', 'igt@xe_exec_threads@threads-many-queues'),
    # VF migration workloads - xe_ccs subtest:
    IgtType.CCS_BLOCK_COPY_COMPRESSED:
      ('n/a', 'igt@xe_ccs@block-copy-compressed'),
    # VF migration workloads - xe_compute_preempt subtest:
    IgtType.COMPUTE_PREEMPT_MANY:
      ('n/a', 'igt@xe_compute_preempt@compute-preempt-many')
    }


class IgtExecutor(ExecutorInterface):
    def __init__(self, target: MachineInterface,
                 test: typing.Union[str, IgtType],
                 num_repeats: int = 1,
                 timeout: int = DEFAULT_TIMEOUT) -> None:
        self.igt_config = target.get_igt_config()

        # TODO ld_library_path not used now, need a way to pass this to guest
        #ld_library_path = f'LD_LIBRARY_PATH={igt_config.lib_dir}'
        runner = posixpath.join(self.igt_config.tool_dir, 'igt_runner')
        testlist = '/tmp/igt_executor.testlist'
        command = f'{runner} {self.igt_config.options} ' \
                  f'--test-list {testlist} {self.igt_config.test_dir} {self.igt_config.result_dir}'
        self.results: typing.Dict[str, typing.Any] = {}
        self.target: MachineInterface = target
        self.igt: str = test if isinstance(test, str) else self.select_igt_variant(target.get_drm_driver_name(), test)

        logger.info("[%s] Execute IGT test: %s", target, self.igt)
        if num_repeats > 1:
            logger.debug("Repeat IGT execution %s times", num_repeats)
            self.igt = (self.igt + '\n') * num_repeats

        self.target.write_file_content(testlist, self.igt)
        self.timeout: int = timeout
        self.proc_result = ProcessResult()
        self.pid: int = self.target.execute(command)

    # Executor interface implementation
    def status(self) -> ProcessResult:
        self.proc_result = self.target.execute_status(self.pid)
        return self.proc_result

    def wait(self) -> ProcessResult:
        self.proc_result = self.target.execute_wait(self.pid, self.timeout)
        return self.proc_result

    def sendsig(self, sig: signal.Signals) -> None:
        self.target.execute_signal(self.pid, sig)

    def terminate(self) -> None:
        self.sendsig(signal.SIGTERM)

    def kill(self) -> None:
        self.sendsig(signal.SIGKILL)

    # IGT specific methods
    def is_running(self) -> bool:
        return not self.status().exited

    def check_results(self) -> bool:
        """Verify IGT test results. Return True for test success, False on fail."""
        if not self.proc_result.exited:
            self.proc_result = self.wait()

        if self.proc_result.exit_code == 0 and self.did_pass():
            logger.debug("[%s] IGT passed", self.target)
            return True

        logger.error("[%s] IGT failed: %s", self.target, self.proc_result)
        return False

    def get_results_log(self) -> typing.Dict:
        # Results are cached
        if self.results:
            logger.debug("Get available IGT results from cache")
            return self.results
        path = posixpath.join(self.igt_config.result_dir, 'results.json')
        result = self.target.read_file_content(path)
        self.results = json.loads(result)
        return self.results

    def did_pass(self) -> bool:
        results = self.get_results_log()
        totals = results.get('totals')
        if not totals:
            return False
        aggregate = totals.get('root')
        if not aggregate:
            return False

        pass_case = 0
        fail_case = 0
        for key in aggregate:
            if key in ['pass', 'warn', 'dmesg-warn']:
                pass_case = pass_case + aggregate[key]
                continue
            fail_case = fail_case + aggregate[key]

        logger.debug("[%s] Full IGT test results:\n%s", self.target, json.dumps(results, indent=4))

        if fail_case > 0:
            logger.error('Test failed!')
            return False

        return True

    def select_igt_variant(self, driver: str, igt_type: IgtType) -> str:
        # Select IGT variant dedicated for a given drm driver: xe or i915
        igt = igt_tests[igt_type]
        return igt[1] if driver == 'xe' else igt[0]


def igt_list_subtests(target: MachineInterface, test_name: str) -> typing.List[str]:
    command = f'{target.get_igt_config().test_dir}{test_name} --list-subtests'
    proc_result = ShellExecutor(target, command).wait()
    if proc_result.exit_code == 0:
        return proc_result.stdout.split("\n")
    return []
