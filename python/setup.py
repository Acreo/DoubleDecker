__author__ = 'eponsko'
from setuptools import setup, find_packages
setup(name='doubledecker',
      version='0.2',
      description='DoubleDecker client and broker',
      url='http://acreo.github.io/DoubleDecker/',
      author='ponsko',
      author_email='ponsko@acreo.se',
      license='LGPLv2.1',
      scripts=['bin/ddbroker.py', 'bin/ddclient.py','bin/ddkeys.py', ],
      packages = find_packages(),
      requires=['zmq', 'nacl'])
