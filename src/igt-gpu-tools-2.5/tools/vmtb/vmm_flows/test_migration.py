# SPDX-License-Identifier: MIT
# Copyright © 2024-2026 Intel Corporation

import enum
import logging
import random
import time
from dataclasses import dataclass
from typing import List, Tuple

import pytest

from bench import exceptions
from bench.configurators.vgpu_profile_config import VfProvisioningMode, VfSchedulingMode
from bench.executors.gem_wsim import ONE_CYCLE_DURATION_MS, PREEMPT_10MS_WORKLOAD, GemWsim
from bench.executors.igt import IgtExecutor, IgtType
from bench.executors.shell import ShellExecutor
from bench.helpers.helpers import (cmd_run_check, driver_check,
                                   duplicate_vm_image, igt_check,
                                   igt_run_check, modprobe_driver_run_check)
from bench.machines.host import Host
from bench.machines.virtual.vm import VirtualMachine
from vmm_flows.conftest import (VmmTestingConfig, VmmTestingSetup,
                                idfn_test_config)

logger = logging.getLogger(__name__)

IGT_INIT_DELAY = 6 # Time between WL start and VM pause (pre-save)
IGT_RESTORE_DELAY = 3 # Time between VM resume and WL status check (post-restore)
MS_IN_SEC = 1000


# Full configuration variant: 1xVF, 2xVF and MAXxVF with auto and vGPU profiles provisioning
# TODO: add max VFs variants
test_variants_full = [(1, VfProvisioningMode.AUTO, VfSchedulingMode.DEFAULT_PROFILE),
                      (2, VfProvisioningMode.AUTO, VfSchedulingMode.DEFAULT_PROFILE),
                      (1, VfProvisioningMode.VGPU_PROFILE, VfSchedulingMode.DEFAULT_PROFILE),
                      (2, VfProvisioningMode.VGPU_PROFILE, VfSchedulingMode.DEFAULT_PROFILE)]


# Basic configuration variant: 1xVF and 2xVF with auto provisioning
test_variants_basic = [(1, VfProvisioningMode.AUTO, VfSchedulingMode.DEFAULT_PROFILE),
                       (2, VfProvisioningMode.AUTO, VfSchedulingMode.DEFAULT_PROFILE)]


# vGPU profiles configuration variant: 1xVF and 2xVF with vGPU profiles provisioning
test_variants_profiles = [(1, VfProvisioningMode.VGPU_PROFILE, VfSchedulingMode.DEFAULT_PROFILE),
                          (2, VfProvisioningMode.VGPU_PROFILE, VfSchedulingMode.DEFAULT_PROFILE)]


@dataclass
class MigrationWorkloadWsim:
    workload_file: str # Wsim workload descriptor file
    num_clients: int # Fork N clients emitting the workload simultaneously
    num_repeats: int # How many times to emit the workload

    def __str__(self) -> str:
        return f'WL:{self.workload_file}-(C:{self.num_clients} R:{self.num_repeats})'


# VF busy migration WSIM workloads (payload for TestBusyMigrationWsim[N]):
wsim_idle_app = MigrationWorkloadWsim('idle_ctxs', 1, 1)
wsim_short_preempt = MigrationWorkloadWsim('short_preempt', 1, 4000) # 5ms * 4000 (20s)
wsim_short_nonpreempt = MigrationWorkloadWsim('short_nonpreempt', 1, 4000)
wsim_long_preempt = MigrationWorkloadWsim('long_preempt', 1, 200) # 100ms * 200 (20s)
wsim_long_nonpreempt = MigrationWorkloadWsim('long_nonpreempt', 1, 200)


@dataclass
class MigrationWorkloadIgt:
    igt_test: IgtType # IGT test type
    num_repeats: int = 1 # Number of repeats of the IGT test (calibrated in runtime)

    def __str__(self) -> str:
        return f'WL:{self.igt_test}'

# VF busy migration IGT workloads (payload for TestBusyMigrationIgt[M]):
# xe_exec_reset/long_spin subtests:
# Average exec time: 12-13s - execute 1x
igt_exec_reset_long_spin_many_preempt = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT)
igt_exec_reset_long_spin_many_preempt_media = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_MEDIA)
igt_exec_reset_long_spin_many_preempt_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_THREADS)
igt_exec_reset_long_spin_many_preempt_gt0_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT0_THREADS)
# Average exec time: 6-7s - execute 2x
igt_exec_reset_long_spin_many_preempt_gt1_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_MANY_PREEMPT_GT1_THREADS)

# Average exec time: 12-13s - execute 1x
igt_exec_reset_long_spin_reuse_many_preempt = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT)
igt_exec_reset_long_spin_reuse_many_preempt_media = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_MEDIA)
igt_exec_reset_long_spin_reuse_many_preempt_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_THREADS)
igt_exec_reset_long_spin_reuse_many_preempt_gt0_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT0_THREADS)
# Average exec time: 6-7s  execute 2x
igt_exec_reset_long_spin_reuse_many_preempt_gt1_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_REUSE_MANY_PREEMPT_GT1_THREADS)

# Average exec time: 12-13s - execute 1x
igt_exec_reset_long_spin_sys_reuse_many_preempt_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_SYS_REUSE_MANY_PREEMPT_THREADS)
igt_exec_reset_long_spin_comp_reuse_many_preempt_threads = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_LONG_SPIN_COMP_REUSE_MANY_PREEMPT_THREADS)

# xe_exec_reset/cancel subtests:
# Average exec time: 5-7s  execute 2x
igt_exec_reset_cancel = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_CANCEL)
igt_exec_reset_cancel_preempt = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_CANCEL_PREEMPT)
# Average exec time: 10-15s  execute 1x
igt_exec_reset_cancel_timeslice_preempt = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_CANCEL_TIMESLICE_PREEMPT)
# Average exec time: 20-25s  execute 1x
igt_exec_reset_cancel_timeslice_many_preempt = MigrationWorkloadIgt(
    IgtType.EXEC_RESET_CANCEL_TIMESLICE_MANY_PREEMPT)

# xe_exec_threads subtests (short, execute in a loop):
# Average exec time: <500ms
igt_exec_threads_basic = MigrationWorkloadIgt(
    IgtType.EXEC_THREADS_BASIC)
igt_exec_threads_bal_basic = MigrationWorkloadIgt(
    IgtType.EXEC_THREADS_BAL_BASIC)
# Average exec time: 1-2s
igt_exec_threads_cm_userptr_invalidate = MigrationWorkloadIgt(
    IgtType.EXEC_THREADS_CM_USERPTR_INVALIDATE)
igt_exec_threads_bal_mixed_userptr_invalidate = MigrationWorkloadIgt(
    IgtType.EXEC_THREADS_BAL_MIXED_USERPTR_INVALIDATE)
# Average exec time: 1-4s
igt_exec_threads_many_queues = MigrationWorkloadIgt(
    IgtType.EXEC_THREADS_MANY_QUEUES)

# xe_ccs subtest (short, execute in a loop):
# Average exec time: 200-600ms
igt_ccs_block_copy_compressed = MigrationWorkloadIgt(
    IgtType.CCS_BLOCK_COPY_COMPRESSED)

# xe_compute_preempt subtest (short, execute in a loop):
# Average exec time: 1.8-2s
igt_compute_preempt_many = MigrationWorkloadIgt(
    IgtType.COMPUTE_PREEMPT_MANY)


class BaseTestBusyMigration:
    """Base class for busy migration tests (with workload executed).

    The class provides implementation for VF save and restore subtests,
    supports parametrization with a different VMs number and various IGT workload types.

    Dedicated for inheritance by separate child test classes with specific workload setup
    to avoid bulk dynamic test variants execution with the same VM setup.
    """

    # State save result flag: executing test_restore depends on prior test_save success
    test_save_failed = True

    def __calibrate_igt_wl(self, vm: VirtualMachine, igt_wl: MigrationWorkloadIgt):
        logger.info("Starting %s test loop calibration for migration workload", igt_wl.igt_test)
        igt_exec =  IgtExecutor(vm, igt_wl.igt_test)
        assert igt_exec.check_results(), 'Calibration IGT run failed'

        results_log = igt_exec.get_results_log()
        igt_exec_time: float = round(results_log['time_elapsed']['end'] - results_log['time_elapsed']['start'], 3)

        # Adjust IGT workload loop to execute longer than pre-save wait (with additional margins)
        if igt_exec_time < IGT_INIT_DELAY + 2:
            igt_wl.num_repeats = int(IGT_INIT_DELAY * 2 / igt_exec_time) + 1

        logger.debug("Calibrated IGT workload loop: %s iteration(s) x ~%ss", igt_wl.num_repeats, igt_exec_time)

    @pytest.fixture(scope='class', name='run_source_workload')
    def fixture_run_source_workload(self, setup_vms, set_migration_wl):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source
        migration_wl = set_migration_wl # WSIM/IGT Workload variant

        if isinstance(migration_wl, MigrationWorkloadWsim):
            wsim_file_path = ts.wsim_wl_dir / f'{migration_wl.workload_file}.wsim' # Workload descriptor file path
            if not wsim_file_path.exists():
                logger.error("gem_wsim workload file %s not available!", wsim_file_path)
                raise exceptions.GemWsimError(f'gem_wsim workload file {wsim_file_path} not available!')

            # Run IGT wsim workload in pre-migration and check completion in post-migration
            return GemWsim(vm_src, migration_wl.num_clients, migration_wl.num_repeats, workload=wsim_file_path)

        if isinstance(migration_wl, MigrationWorkloadIgt):
            self.__calibrate_igt_wl(vm_src, migration_wl)
            return IgtExecutor(vm_src, migration_wl.igt_test, migration_wl.num_repeats)

        logger.error("Invalid workload type passed to run_source_workload fixture")
        raise exceptions.BenchError('Invalid workload type passed to run_source_workload fixture')

    @pytest.fixture(scope='function', name='setup_destination_vm')
    def fixture_setup_destination_vm(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0]  # First VM as a source
        vm_dst: VirtualMachine = ts.get_vm[-1] # Last VM as a destination
        num_vms = ts.get_num_vms()

        if num_vms == 1:
            logger.debug("Single VM: the same source and destination VM instance")
            assert vm_src == vm_dst
            return vm_dst

        logger.debug("Multiple VMs: reload destination VM with the source image (with state snapshot)")

        if vm_src.is_running():
            # QMP 'quit' is used for paused VM (cannot be powered off via guest-agent)
            vm_src.quit()

        if vm_dst.is_running():
            vm_dst.quit()
            while vm_dst.is_running():
                time.sleep(1) # VM usually doesn't terminate immediately

        # Re-start destination VM with an image containing a state snapshot
        vm_dst.set_migration_source(vm_src.image)
        vm_dst.poweron()

        return vm_dst

    def test_save(self, setup_vms, run_source_workload):
        logger.info("Test VM busy migration: state save")
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source

        logger.debug("Execute migration in-flight workload on source VM")
        migration_wl = run_source_workload
        time.sleep(IGT_INIT_DELAY)
        assert migration_wl.is_running(), 'IGT/wsim migration workload is not running on source VM'

        # Pause VM and save snapshot
        logger.debug("Pause execution and save source VM state")
        try:
            vm_src.pause()
            vm_src.save_state()
        except exceptions.GuestError as exc:
            logger.error("State save error: %s", exc)
            assert False, 'VF migration failed on save'

        logger.debug("Resume execution on source VM")
        vm_src.resume()

        assert migration_wl.check_results(), 'VF migration workload failed on source VM (post-save)'

        if ts.get_num_vms() > 1:
            logger.debug("Multiple VMs: shutdown source VM")
            vm_src.poweroff()

        BaseTestBusyMigration.test_save_failed = False

    def test_restore(self, setup_vms, setup_destination_vm, run_source_workload):
        logger.info("Test VM busy migration: state restore")
        if BaseTestBusyMigration.test_save_failed:
            logger.error("State save failed - restore is pointless (fail immediately)")
            assert False, 'test_save subtest failed - do not execute test_restore'

        ts: VmmTestingSetup = setup_vms
        vm_dst: VirtualMachine = setup_destination_vm
        migration_wl = run_source_workload # Get an instance of the IGT WL started in a save test

        # Patch the source IgtExecutor/GemWsim instance with the current VM
        migration_wl.target = vm_dst
        if isinstance(migration_wl, IgtExecutor):
            # Clear IGT test results cache - remove post-save source VM results
            # TODO: implement common IgtExecutor/GemWsim results clear interface to avoid instance type check
            migration_wl.results.clear()

        # Load the source state snapshot
        logger.debug("Restore source state on the destination VM")
        vm_dst.load_state()
        vm_dst.resume()

        # TODO: add sync to VM class
        sync_value = random.randint(1, 0xFFFF)
        assert vm_dst.ga.sync(sync_value)['return'] == sync_value

        assert migration_wl.is_running(), 'IGT/wsim migration workload is not running on destination VM'
        time.sleep(IGT_RESTORE_DELAY)

        assert migration_wl.check_results(), 'VF migration workload failed on destination VM (post-restore)'

        logger.debug("Check driver health on host and destination VM")
        assert driver_check(ts.host)
        assert driver_check(vm_dst)


@pytest.fixture(scope='class', name='set_migration_wl')
def fixture_set_migration_wl(request):
    """Set IGT/wsim descriptor file used as a migration workload in a TestBusyMigration[WL]."""
    # Wsim workload variant provided as MigrationWorkload data class instance
    return request.param


def idfn_workload(workload: MigrationWorkloadWsim):
    """Add workload name to a test config ID in parametrized tests
    (e.g. test_something[2VF-WL:workload_type-C:n-R:m].
    """
    return str(workload)


def set_test_config(test_variants: List[Tuple[int, VfProvisioningMode, VfSchedulingMode]],
                    max_vms: int = 2, wa_reduce_vf_lmem: bool = False) -> List[VmmTestingConfig]:
    """Helper function to provide a parametrized test with a list of test configuration variants."""
    test_configs: List[VmmTestingConfig] = []

    for config in test_variants:
        (num_vfs, provisioning_mode, scheduling_mode) = config
        test_configs.append(VmmTestingConfig(num_vfs, max_vms, provisioning_mode, scheduling_mode,
                                             wa_reduce_vf_lmem=wa_reduce_vf_lmem))

    return test_configs


# Busy migration TCs with WSIM workload
@pytest.mark.parametrize('set_migration_wl', [wsim_short_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full), ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationWsim1(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing short (5ms) preemptable batches.

    IGT/WSIM workload initiated pre-migration starts firing short submissions on each engine and
    during the execution VM state is migrated (VM state snapshot is saved, then restored).
    In the post-migration some additional batches are submitted.
    Executed in the following VM number variants:
    - single VF/VM: same VM acts as a source and destination.
    - multiple VFs/VMs: the workload execution is initiated on the source VM,
      then migrated and verified on the other, destination one.
    """


@pytest.mark.parametrize('set_migration_wl', [wsim_short_nonpreempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationWsim2(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing short (5ms) non-preemptable batches.
    Similar to TestBusyMigrationShort subtest, but emits non-preemptable batches.
    """


@pytest.mark.parametrize('set_migration_wl', [wsim_long_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationWsim3(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing quite long (100ms) but preemptable batches.

    IGT/WSIM workload initiated pre-migration starts firing relatively long submissions and
    during the execution VM state is migrated (VM state snapshot is saved, then restored).
    In the post-migration some additional batches are submitted.
    Executed in the following VM number variants:
    - single VF/VM: same VM acts as a source and destination.
    - multiple VFs/VMs: the workload execution is initiated on the source VM,
      then migrated and verified on the other, destination one.
    """


# TODO: convert to negative scenario.
# Test is expected to fail because non-premptable workload execution time > PT (VLK-81241)
@pytest.mark.parametrize('set_migration_wl', [wsim_long_nonpreempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationWsim4(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing quite long (100ms) non-preemptable batches.
    Similar to TestBusyMigrationLong subtest, but emits non-preemptable batches.
    """


@pytest.mark.parametrize('set_migration_wl', [wsim_idle_app],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestIdleAppMigration(BaseTestBusyMigration):
    """Save-restore VM state with an idle VF but user application attached (contexts created).

    IGT/WSIM workload initiated pre-migration creates multiple user contexts and
    does short submission on each but is idle during a save-restore operation,
    then resumes post-migration to do more submissions on previously created contexts.
    Executed in the following VM number variants:
    - single VF/VM: same VM acts as a source and destination.
    - multiple VFs/VMs: the workload execution is initiated on the source VM,
      then migrated and verified on the other, destination one.
    """

# Busy migration TCs with IGT workload
@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_many_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset1(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-many-preempt."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_many_preempt_media],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset2(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-many-preempt-media."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_many_preempt_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset3(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-many-preempt-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_many_preempt_gt0_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset4(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-many-preempt-gt0-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_many_preempt_gt1_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset5(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-many-preempt-gt1-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_reuse_many_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset6(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-reuse-many-preempt."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_reuse_many_preempt_media],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset7(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-reuse-many-preempt-media."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_reuse_many_preempt_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset8(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-reuse-many-preempt-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_reuse_many_preempt_gt0_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset9(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-reuse-many-preempt-gt0-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_reuse_many_preempt_gt1_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset10(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-reuse-many-preempt-gt1-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_sys_reuse_many_preempt_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset11(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-sys-reuse-many-preempt-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_long_spin_comp_reuse_many_preempt_threads],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset12(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@long-spin-comp-reuse-many-preempt-threads."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_cancel],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset13(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@cancel."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_cancel_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset14(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@cancel-preempt."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_cancel_timeslice_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset15(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@cancel-timeslice-preempt."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_reset_cancel_timeslice_many_preempt],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecReset16(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_reset@cancel-timeslice-many-preempt."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_threads_basic],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecThreads1(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_threads@threads-basic."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_threads_bal_basic],
                         ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecThreads2(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_threads@threads-bal-basic."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_threads_cm_userptr_invalidate],
                        ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecThreads3(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_threads@threads-cm-userptr-invalidate."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_threads_many_queues],
                        ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_full),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecThreads4(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_threads@threads-many-queues."""


@pytest.mark.parametrize('set_migration_wl', [igt_exec_threads_bal_mixed_userptr_invalidate],
                        ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtExecThreads5(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_exec_threads@threads-bal-mixed-userptr-invalidate."""


@pytest.mark.parametrize('set_migration_wl', [igt_ccs_block_copy_compressed],
                        ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtCcs(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_ccs@block-copy-compressed."""


@pytest.mark.parametrize('set_migration_wl', [igt_compute_preempt_many],
                        ids=idfn_workload, indirect=['set_migration_wl'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestBusyMigrationIgtComputePreempt(BaseTestBusyMigration):
    """Save-restore VM state with VF busy executing IGT xe_compute_preempt@compute-preempt-many (CCS path)."""


@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestIdleMigration:
    """Save-restore VM state with an idle VF and no user application attached.

    IGT workload initiated and ended twice: pre- and post-migration, but not executing during a save-restore operation.
    Test setup:
    - NxVFs running NxVM instances (first (VM[0]) acts as source and a last (VM[N-1] as a destination)
    - platform provisioned with the relevant vGPU profile M[N] (ATSM, ADLP) or C[N] (PVC)
    - VF state is saved on the source VM and then restored on the destination VM instance
      (in case of a single VF variant, source and destination is the same VM instance)
    """

    @pytest.fixture(scope='function', name='setup_destination_vm')
    def fixture_setup_destination_vm(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0]  # First VM as a source
        vm_dst: VirtualMachine = ts.get_vm[-1] # Last VM as a destination
        num_vms = ts.get_num_vms()

        if num_vms == 1:
            logger.debug("Single VM: the same source and destination VM instance")
            assert vm_src == vm_dst
            return vm_dst

        logger.debug("Multiple VMs: reload destination VM with the source image (with state snapshot)")

        if vm_src.is_running():
            # QMP 'quit' is used for paused VM (cannot be powered off via guest-agent)
            vm_src.quit()

        if vm_dst.is_running():
            vm_dst.quit()
            while vm_dst.is_running():
                time.sleep(1) # VM usually doesn't terminate immediately

        # Re-start destination VM with an image containing a state snapshot
        vm_dst.set_migration_source(vm_src.image)
        vm_dst.poweron()

        return vm_dst

    def test_save(self, setup_vms):
        logger.info("Test VM idle migration: state save")
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source

        # Run some interactive program (not returning, as vim) to verify state after migration
        src_proc = ShellExecutor(vm_src, 'vim migrate.txt')
        source_proc = vm_src.execute_status(src_proc.pid)
        logger.debug("Source process: %s", source_proc)
        assert source_proc.exited is False, 'Source process is not running'

        logger.debug("Execute pre-migration workload on source VM")
        assert igt_run_check(vm_src, IgtType.EXEC_STORE)

        # Pause VM and save snapshot
        logger.debug("Pause execution and save VM state")
        try:
            vm_src.pause()
            vm_src.save_state()
        except exceptions.GuestError as exc:
            logger.error("State save error: %s", exc)
            assert False, 'VF migration failed on save'

    def test_restore(self, setup_vms, setup_destination_vm):
        logger.info("Test VM idle migration: state restore")
        ts: VmmTestingSetup = setup_vms
        vm_dst: VirtualMachine = setup_destination_vm

        # Load the source state snapshot
        logger.debug("Restore source state on the destination VM")
        vm_dst.load_state()
        vm_dst.resume()

        # Verify program initiated on source VM is stil running after migration
        pgrep_dst = ShellExecutor(vm_dst, 'pgrep -f "vim migrate.txt"')
        pgrep_dst_result = vm_dst.execute_wait(pgrep_dst.pid)
        assert pgrep_dst_result.exit_code == 0, 'Source process (vim) not found'
        restored_proc = vm_dst.execute_status(int(pgrep_dst_result.stdout))
        logger.debug("Restored process: %s", restored_proc)
        assert restored_proc.exited is False, 'Restored process is not running'

        logger.debug("Execute post-migration workload on destination VM")
        assert igt_run_check(vm_dst, IgtType.EXEC_STORE)

        logger.debug("Check driver health on host and destination VM")
        assert driver_check(ts.host)
        assert driver_check(vm_dst)


class ResfixWaitStage(enum.IntEnum):
    # Resfix stopper checkpoints
    VF_MIGRATION_CONTINUE = 0
    VF_MIGRATION_WAIT_BEFORE_RESFIX_START = 1 << 0
    VF_MIGRATION_WAIT_BEFORE_FIXUPS = 1 << 1
    VF_MIGRATION_WAIT_BEFORE_RESTART_JOBS = 1 << 2
    VF_MIGRATION_WAIT_BEFORE_RESFIX_DONE = 1 << 3


class MigrationToRestore(enum.Enum):
    FIRST = 1
    SECOND = 2


@dataclass
class DoubleMigrationConfig:
    resfix_stoppers: ResfixWaitStage # Stage for migration RESFIX stop
    migration_to_restore: MigrationToRestore # Migration snapshot to be restored after doubled save

    def __str__(self) -> str:
        return f'RS:{hex(self.resfix_stoppers)}-MR:{self.migration_to_restore}'


double_migration_1_resfix_1 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESFIX_START, MigrationToRestore.FIRST)
double_migration_1_resfix_2 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_FIXUPS, MigrationToRestore.FIRST)
double_migration_1_resfix_3 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESTART_JOBS, MigrationToRestore.FIRST)
double_migration_1_resfix_4 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESFIX_DONE, MigrationToRestore.FIRST)


double_migration_2_resfix_1 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESFIX_START, MigrationToRestore.SECOND)
double_migration_2_resfix_2 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_FIXUPS, MigrationToRestore.SECOND)
double_migration_2_resfix_3 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESTART_JOBS, MigrationToRestore.SECOND)
double_migration_2_resfix_4 = DoubleMigrationConfig(
    ResfixWaitStage.VF_MIGRATION_WAIT_BEFORE_RESFIX_DONE, MigrationToRestore.SECOND)

class BaseTestDoubleMigration:
    """Base class for double migration tests.
    Test scenario triggers VF re-migrate while the initial restore (resources fixup) is still ongoing.

    Save-load and immediately save again before the initial migration completes (prior to resfix done).
    Post migration resources fixup is delayed via KMD debug hook to initiate the 2nd save.
    Tests Xe KMD corner case where two migration notifications must be handled.
    IGT/WSIM workload is executing during the migration (started prior to 1st save).

    The class provides implementation for VF save and restore-save subtests,
    supports parametrization with a different VMs number and double migration scenario variants.

    Dedicated for inheritance by separate child test classes with specific
    double migration test scenarios configurations:
    - stopping RESFIX in a different stage
    - restoring from initial (1st) or latter (2nd) migration
    """

    def __set_debugfs_resfix_stoppers(self, vm: VirtualMachine, stage: ResfixWaitStage):
        """Set resfix_stoppers:
        predefined checkpoints that allow the migration process to pause at specific stages.
        Each state will pause with a 1-second delay per iteration, continuing until
        its corresponding bit is cleared.
        Debug hook path: /sys/kernel/debug/dri/<card>/gt0/vf/resfix_stoppers
        """
        vf_driver = vm.get_dut().driver
        vf_driver.write_debugfs(f'{vf_driver.debugfs_path}/gt0/vf/resfix_stoppers', str(stage))

        resfix_stoppers = vf_driver.read_debugfs(f'{vf_driver.debugfs_path}/gt0/vf/resfix_stoppers').strip()
        logger.debug("[%s] Set migration resfix stoppers: %s (%s)"
                     "\nPause checkpoints:"
                     "\n\tVF_MIGRATION_WAIT_BEFORE_RESFIX_START: BIT(0)"
                     "\n\tVF_MIGRATION_WAIT_BEFORE_FIXUPS: BIT(1)"
                     "\n\tVF_MIGRATION_WAIT_BEFORE_RESTART_JOBS: BIT(2)"
                     "\n\tVF_MIGRATION_WAIT_BEFORE_RESFIX_DONE: BIT(3)"
                     "\n\tResume execution: 0",
                     vm, resfix_stoppers, bin(int(resfix_stoppers, 16)))

        return int(resfix_stoppers, 16) == stage

    def __is_resfix_stopped(self, vm: VirtualMachine):
        vf_driver = vm.get_dut().driver
        resfix_stoppers = vf_driver.read_debugfs(f'{vf_driver.debugfs_path}/gt0/vf/resfix_stoppers').strip()

        return int(resfix_stoppers, 16) != 0

    @pytest.fixture(scope='function', name='set_resfix_stoppers')
    def fixture_set_resfix_stoppers(self, setup_vms, set_double_migration_config):
        ts: VmmTestingSetup = setup_vms
        migration_config: DoubleMigrationConfig = set_double_migration_config
        vm_src: VirtualMachine = ts.get_vm[0]  # First VM as a source

        return self.__set_debugfs_resfix_stoppers(vm_src, migration_config.resfix_stoppers)

    @pytest.fixture(scope='function', name='clear_resfix_stoppers')
    def fixture_clear_resfix_stoppers(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        yield

        for vm in ts.get_vm:
            if vm.is_running() and self.__is_resfix_stopped(vm):
                logger.info("Teardown fixture - clear remaining resfix stoppers")
                self.__set_debugfs_resfix_stoppers(vm, ResfixWaitStage.VF_MIGRATION_CONTINUE)

    @pytest.fixture(scope='class', name='run_source_workload')
    def fixture_run_source_workload(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source
        migration_wl: MigrationWorkloadWsim = wsim_short_preempt # Workload variant
        wsim_file_path = ts.wsim_wl_dir / f'{migration_wl.workload_file}.wsim' # Workload descriptor file path
        if not wsim_file_path.exists():
            logger.error("gem_wsim workload file %s not available!", wsim_file_path)
            raise exceptions.GemWsimError(f'gem_wsim workload file {wsim_file_path} not available!')

        # Run IGT wsim workload in pre-migration and check completion in post-migration
        return GemWsim(vm_src, migration_wl.num_clients, migration_wl.num_repeats, workload=wsim_file_path)

    @pytest.fixture(scope='function', name='setup_destination_vm')
    def fixture_setup_destination_vm(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0]  # First VM as a source
        vm_dst: VirtualMachine = ts.get_vm[-1] # Last VM as a destination
        num_vms = ts.get_num_vms()

        if num_vms == 1:
            logger.debug("Single VM: the same source and destination VM instance")
            assert vm_src == vm_dst
            vm_dst.pause()
            return vm_dst

        logger.debug("Multiple VMs: reload destination VM with the source image (with state snapshot)")

        if vm_src.is_running():
            # QMP 'quit' is used for paused VM (cannot be powered off via guest-agent)
            vm_src.quit()

        if vm_dst.is_running():
            vm_dst.quit()
            while vm_dst.is_running():
                time.sleep(1) # VM usually doesn't terminate immediately

        # Re-start destination VM with an image containing a state snapshot
        vm_dst.set_migration_source(vm_src.image)
        vm_dst.poweron()

        return vm_dst

    def test_save(self, setup_vms, run_source_workload, set_resfix_stoppers):
        logger.info("Test VM double migration: 1st state save")
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source
        assert set_resfix_stoppers, 'Failed to set migration resfix stoppers'

        logger.debug("Execute throughout-migration workload on source VM")
        migration_wl: GemWsim = run_source_workload
        time.sleep(IGT_INIT_DELAY)
        assert migration_wl.is_running(), 'IGT/wsim migration workload is not running on source VM'

        # Pause source VM and save snapshot
        logger.debug("Pause execution and save source VM state (snapshot #1)")
        try:
            vm_src.pause()
            vm_src.save_state() # snapshot #1
        except exceptions.GuestError as exc:
            logger.error("State save error: %s", exc)
            assert False, 'VF migration failed on save'

    def test_restore_save(self, setup_destination_vm, run_source_workload,
                          set_double_migration_config, clear_resfix_stoppers):
        logger.info("Test VM double migration: state restore and 2nd save prior recovery is done")
        vm_dst: VirtualMachine = setup_destination_vm
        migration_wl: GemWsim = run_source_workload # Get an instance of the IGT WL started in a save test
        migration_config: DoubleMigrationConfig = set_double_migration_config

        # Patch the source IgtExecutor instance with the current VM and clear results cache
        migration_wl.target = vm_dst

        # Load the source state snapshot
        logger.debug("Restore source state on the destination VM (snapshot #1)")
        vm_dst.load_state() # snapshot #1
        vm_dst.resume()

        time.sleep(3) # Wait a bit for the migration recovery fires
        logger.debug("Save 2nd VM state (snapshot #2) while the 1st migration recovery still in progress")

        if migration_config.migration_to_restore is MigrationToRestore.FIRST:
            # VM pause/resume is implicitly called by save,
            # snapshot #1 recovery is continued immediately after snapshot #2 save completes
            vm_dst.save_state() # snapshot #2
            logger.info("Continue source VM state recovery (snapshot #1)")

        if migration_config.migration_to_restore is MigrationToRestore.SECOND:
            # Include explicit VM pause/resume, snapshot #2 load shall immediately follow it's save,
            # to not allow continuation of state recovery of snapshot #1.
            vm_dst.pause()
            vm_dst.save_state() # snapshot #2
            logger.info("Load state and re-start state recovery of 2nd saved state (snapshot #2)")
            vm_dst.load_state() # snapshot #2
            vm_dst.resume()

        logger.info("Continue migration recovery - clear resfix stoppers")
        self.__set_debugfs_resfix_stoppers(vm_dst, ResfixWaitStage.VF_MIGRATION_CONTINUE)

        logger.debug("Check migration in-flight workload after destination VM save")
        time.sleep(IGT_RESTORE_DELAY)
        assert migration_wl.check_results(), 'VF migration workload failed on destination VM (post-restore)'


@pytest.fixture(scope='class', name='set_double_migration_config')
def fixture_set_double_migration_config(request):
    """Set migration recovery wait stage for double migration test and number of snapshot to restore."""
    # Provide list of DoubleMigrationConfig instances to setup the test.
    return request.param


def idfn_double_migration(config: DoubleMigrationConfig):
    """Add double migration settings to a test config ID in parametrized tests
    (e.g. test_something[2VF-RS:resfix_stopper-MR:snapshot_to_restore].
    """
    return str(config)


@pytest.mark.parametrize('set_double_migration_config', [double_migration_1_resfix_1],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration1Resfix1(BaseTestDoubleMigration):
    """Double migration test restoring the first snapshot (the former, initial migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> continue to recover #1
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESFIX_START (BIT0) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_1_resfix_2],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration1Resfix2(BaseTestDoubleMigration):
    """Double migration test restoring the first snapshot (the former, initial migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> continue to recover #1
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_FIXUPS (BIT1) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_1_resfix_3],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration1Resfix3(BaseTestDoubleMigration):
    """Double migration test restoring the first snapshot (the former, initial migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> continue to recover #1
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESTART_JOBS (BIT2) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_1_resfix_4],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration1Resfix4(BaseTestDoubleMigration):
    """Double migration test restoring the first snapshot (the former, initial migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> continue to recover #1
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESFIX_DONE (BIT3) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_2_resfix_1],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration2Resfix1(BaseTestDoubleMigration):
    """Double migration test restoring the second snapshot (the latter migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> load and recover #2
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESFIX_START (BIT0) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_2_resfix_2],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration2Resfix2(BaseTestDoubleMigration):
    """Double migration test restoring the second snapshot (the latter migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> load and recover #2
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_FIXUPS (BIT1) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_2_resfix_3],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration2Resfix3(BaseTestDoubleMigration):
    """Double migration test restoring the second snapshot (the latter migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> load and recover #2
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESTART_JOBS (BIT2) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('set_double_migration_config', [double_migration_2_resfix_4],
                        ids=idfn_double_migration, indirect=['set_double_migration_config'])
@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_profiles, wa_reduce_vf_lmem=True),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestDoubleMigration2Resfix4(BaseTestDoubleMigration):
    """Double migration test restoring the second snapshot (the latter migration):
    save snapshot #1 -> load snapshot #1 -> save snapshot #2 (during #1 recovery) -> load and recover #2
    Stop resfix on VF_MIGRATION_WAIT_BEFORE_RESFIX_DONE (BIT3) checkpoint to initiate 2nd save.

    W/A: reduce VF VRAM quota to speed up the 2nd save (to avoid time-out).
    """


@pytest.mark.parametrize('setup_vms', set_test_config(test_variants_basic),
                         ids=idfn_test_config, indirect=['setup_vms'])
class TestCheckpoint:
    """Verify a state can be saved for the future use and then loaded at the previous checkpoint."""

    @pytest.fixture(scope='function', name='setup_destination_vm')
    def fixture_setup_destination_vm(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0]  # First VM as a source
        vm_dst: VirtualMachine = ts.get_vm[-1] # Last VM as a destination
        num_vms = ts.get_num_vms()

        if num_vms == 1:
            logger.debug("Single VM: the same source and destination VM instance")
            assert vm_src == vm_dst
            return vm_dst

        logger.debug("Multiple VMs: restart destination VM with the source image (with state checkpoint)")
        vm_dst.poweroff()
        # Source qcow2 must be copied because multiple VMs cannot run with the same image file
        vm_dst.set_migration_source(duplicate_vm_image(vm_src.image))
        vm_dst.poweron()
        vm_dst.resume()
        assert modprobe_driver_run_check(vm_dst)

        return vm_dst

    @pytest.fixture(scope='class', name='run_source_workload')
    def fixture_run_source_workload(self, setup_vms):
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source

        # Run IGT workload to check before and after a state checkpoint
        return IgtExecutor(vm_src, IgtType.SPIN_BATCH)

    def test_save(self, setup_vms, run_source_workload):
        logger.info("Test VM state checkpoint save")
        ts: VmmTestingSetup = setup_vms
        vm_src: VirtualMachine = ts.get_vm[0] # First VM as source
        igt_src: IgtExecutor = run_source_workload

        # Save state checkpoint
        logger.debug("Save VM state checkpoint")
        try:
            vm_src.save_state()
        except exceptions.GuestError as exc:
            logger.error("State save error: %s", exc)
            assert False, 'VF migration failed on save'

        # Verify workload submitted prior to the state checkpoint succeeds
        assert igt_check(igt_src), 'Source IGT workload has failed'

        logger.debug("Check driver health on host and source VM")
        assert driver_check(ts.host)
        assert driver_check(vm_src)

    def test_load(self, setup_vms, setup_destination_vm, run_source_workload):
        logger.info("Test VM state checkpoint load")
        ts: VmmTestingSetup = setup_vms
        vm_dst: VirtualMachine = setup_destination_vm
        igt_src: IgtExecutor = run_source_workload # Get an instance of the IGT WL started in a save test

        # Patch the source IgtExecutor instance with the current VM and clear results cache
        igt_src.target = vm_dst
        igt_src.results.clear()

        # Workload submitted before the checkpoint should not be active before load
        logger.debug("Verify IGT workload is not executing prior to the state restore (expected pgrep error)")
        assert not cmd_run_check(vm_dst, 'pgrep igt_runner'), 'IGT workload is (unexpectedly) running'

        # Load previously saved state checkpoint and resume on destination VM
        logger.debug("Load VM state checkpoint")
        vm_dst.load_state()

        # Workload submitted before the checkpoint should be restored in running state after load
        logger.debug("Verify IGT workload is executing again after the state restore")
        assert not igt_src.status().exited, 'IGT workload is not running after checkpoint load'
        assert igt_check(igt_src), 'IGT workload loaded on checkpoint has failed'

        logger.debug("Check driver health on host and destination VM")
        assert driver_check(ts.host)
        assert driver_check(vm_dst)


def test_2vm_pause_resume(create_1host_2vm):
    """
    VM/VF pause-resume does not affect workload execution:
     - 2xVFs running 2xVM instance
     - both VFs auto-provisioned, running IGT workloads
     - 1st VM/VF is paused and resumed (but VF state is not saved/loaded)
     - 2nd VM/VF workload should not be interrupted
     - IGT workloads shall finish successfully on both VMs
    """
    ts: VmmTestingSetup = create_1host_2vm
    host: Host = ts.host
    vm0: VirtualMachine = ts.get_vm[0]
    vm1: VirtualMachine = ts.get_vm[1]
    assert driver_check(host)

    num_vfs = ts.testing_config.num_vfs
    assert ts.get_dut().create_vf(num_vfs) == num_vfs

    vf1, vf2 = ts.get_dut().get_vfs_bdf(1, 2)
    vm0.assign_vf(vf1)
    vm1.assign_vf(vf2)
    ts.poweron_vms()

    pause_vf_num = 1

    assert modprobe_driver_run_check(vm0)
    assert modprobe_driver_run_check(vm1)

    logger.debug("Submit IGT WL (gem_wsim) on VM0")
    iterations = 3000 # 3k iterations of 10ms WLs give 30s total expected time
    expected_elapsed_sec = ONE_CYCLE_DURATION_MS * iterations / MS_IN_SEC
    gem_wsim_vm0 = GemWsim(vm0, 1, iterations, PREEMPT_10MS_WORKLOAD)

    # Allow wsim WL to run some time
    time.sleep(IGT_INIT_DELAY)
    assert gem_wsim_vm0.is_running()

    logger.debug("Submit IGT WL (gem_spin_batch) on VM1")
    igt_vm1 = IgtExecutor(vm1, IgtType.SPIN_BATCH)

    # Special handling of pausing VMs with infinite ExecQuanta - refer to SAS for details
    logger.debug("Set VF1 EQ/PF before the pause")
    ts.get_dut().driver.set_exec_quantum_ms(pause_vf_num, 1)
    ts.get_dut().driver.set_preempt_timeout_us(pause_vf_num, 100)

    logger.debug("Pause execution on VM0/VF1")
    vm0.pause()

    assert igt_check(igt_vm1)
    logger.debug("VM1 IGT WL (not paused) finished successfully")

    logger.debug("Resume execution on VM0/VF1")
    vm0.resume()

    logger.debug("Reset VF1 EQ/PF to the initial values (infinite) after resume")
    ts.get_dut().driver.set_exec_quantum_ms(pause_vf_num, 0)
    ts.get_dut().driver.set_preempt_timeout_us(pause_vf_num, 0)

    result_vm0 = gem_wsim_vm0.wait_results() # Throws exception on wsim fail
    assert expected_elapsed_sec * 0.8 < result_vm0.elapsed_sec < expected_elapsed_sec * 1.5
    logger.debug("VM0 IGT WL (paused-resumed) finished successfully")

    # Check host and VM health status after pause-resume transition
    assert driver_check(host)
    assert driver_check(vm0)
    assert driver_check(vm1)


def test_1vm_save_restore_no_driver(create_1host_1vm):
    """
    Save/restore single VM state with no guest driver loaded:
     - 1xVFs running 1xVM instance (single VM acts as source and destination)
     - platform provisioned with vGPU profile M1 (ATSM, ADLP) or C1 (PVC)
     - VF state saved and then restored on the same VM instance
     - driver probed on VM after the resume, IGT workload executed
    """
    ts: VmmTestingSetup = create_1host_1vm
    host: Host = ts.host
    vm: VirtualMachine = ts.get_vm[0]
    assert driver_check(host)

    num_vfs = ts.testing_config.num_vfs
    assert ts.get_dut().create_vf(num_vfs) == num_vfs

    vf = ts.get_dut().get_vf_bdf(1)
    vm.assign_vf(vf)

    vm.poweron()

    # Run some interactive program (not returning, as vim) to verify state after migration
    src_proc = ShellExecutor(vm, 'vim migrate.txt')
    src_pid = src_proc.pid

    # Pause VM and save snapshot
    logger.debug("Pause execution and save VM state")
    try:
        vm.pause()
        vm.save_state()
    except exceptions.GuestError as exc:
        logger.error("State save error: %s", exc)
        assert False, 'VF migration failed on save'

    # Load previously saved snapshot and resume the same VM
    logger.debug("Load state on the same VM instance")
    vm.load_state()
    vm.resume()

    # Verify program initiated on source VM is stil running after migration
    migrated_proc = vm.execute_status(src_pid)
    logger.debug("Migrated process: %s", migrated_proc)
    assert migrated_proc.exited is False, 'Migrated process is not running after VM snapshot load'

    logger.debug("Probe driver and execute workload on VM")
    assert modprobe_driver_run_check(vm)
    assert igt_run_check(vm, IgtType.EXEC_STORE)

    logger.debug("Check driver health on host and VM")
    assert driver_check(host)
    assert driver_check(vm)
