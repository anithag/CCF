# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the Apache 2.0 License.

import e2e_logging
import reconfiguration
import recovery
import rekey

from inspect import signature, Parameter

# TODO: For now, these are hardcoded. Indeed, late join after recovery is not yet supported.
# https://github.com/microsoft/CCF/issues/315
tests = [
    reconfiguration.test_add_node,
    reconfiguration.test_add_node_from_backup,
    reconfiguration.test_add_as_many_pending_nodes,
    reconfiguration.test_add_node_untrusted_code,
    reconfiguration.test_retire_node,
    e2e_logging.test,
    e2e_logging.test_update_lua,
    recovery.test,
    rekey.test,
    recovery.test,
    reconfiguration.test_retire_node,
]

#
# Test functions are expected to be in the following format:
#
# @requirements_decorator (see suite/test_requirements.py)
def test_example(network, args):
    # Test logic, e.g. issuing transaction or adding a node
    return network


def test_name(test):
    return f"{test.__module__}.{test.__name__}"


def validate_tests_signature(suite):
    """
    Validates that the test functions signatures are in the correct format
    """
    valid_sig = signature(test_example)

    for test in suite:
        sig = signature(test)

        assert len(sig.parameters) >= len(
            valid_sig.parameters
        ), f"{test_name(test)} should have at least {len(valid_sig.parameters)} parameters (only has {len(sig.parameters)})"

        p_index = 0
        for p, v in zip(sig.parameters.items(), valid_sig.parameters.items()):
            assert (
                p[0] == v[0]
            ), f'Signature of {test_name(test)} does not contain "{v[0]}" parameter in the right order'
            p_index += 1

        for p in list(sig.parameters.values())[p_index:]:
            assert (
                p.default is not Parameter.empty
            ), f'Signature of {test_name(test)} includes custom non-defaulted parameter "{p}"'