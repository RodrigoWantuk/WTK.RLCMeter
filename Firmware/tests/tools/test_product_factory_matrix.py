"""Reject mislabeled size evidence before expensive firmware builds."""
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('matrix', Path(__file__).resolve().parents[2] /
                                           'tools/product_factory_matrix.py')
matrix = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(matrix)


class MatrixConfigurationTests(unittest.TestCase):
    def test_exact_configuration_and_old_cmake_uninitialized_override(self):
        sha = '334798c54e372b1267b01156bd742e20a4ea1460'
        self.assertEqual(matrix.pin_archived_banner(
            'COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD', sha),
            'COMMAND ${CMAKE_COMMAND} -E echo 334798c54e37')
        for text, revision in (('unexpected detector', sha), ('', 'HEAD')):
            with self.assertRaises(ValueError):
                matrix.pin_archived_banner(text, revision)
        for factory in ('OFF', 'ON'):
            for curves in ('OFF', 'ON'):
                cache = f'WTK_PRODUCT_FACTORY_PROVISIONED:UNINITIALIZED={factory}\nWTK_ENABLE_SUPPLEMENTARY_CURVES:BOOL={curves}\n'
                matrix.verify_configuration(cache, factory, curves)

    def test_literal_shell_variable_stale_or_missing_flag_rejected(self):
        for value in ('$taskCurves', 'ON', ''):
            with self.assertRaisesRegex(ValueError, 'SUPPLEMENTARY_CURVES'):
                matrix.verify_configuration('WTK_PRODUCT_FACTORY_PROVISIONED:BOOL=OFF\n' +
                    f'WTK_ENABLE_SUPPLEMENTARY_CURVES:BOOL={value}\n', 'OFF', 'OFF')
        with self.assertRaisesRegex(ValueError, 'FACTORY_PROVISIONED'):
            matrix.verify_configuration('WTK_ENABLE_SUPPLEMENTARY_CURVES:BOOL=OFF\n', 'ON', 'OFF')


if __name__ == '__main__':
    unittest.main()
