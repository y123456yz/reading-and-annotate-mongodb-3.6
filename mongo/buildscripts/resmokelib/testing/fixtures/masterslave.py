"""
Main/subordinate fixture for executing JSTests against.
"""

from __future__ import absolute_import

import os.path

import pymongo
import pymongo.errors

from . import interface
from . import standalone
from ... import config
from ... import utils


class MainSubordinateFixture(interface.ReplFixture):
    """
    Fixture which provides JSTests with a main/subordinate deployment to
    run against.
    """

    def __init__(self,
                 logger,
                 job_num,
                 mongod_executable=None,
                 mongod_options=None,
                 main_options=None,
                 subordinate_options=None,
                 dbpath_prefix=None,
                 preserve_dbpath=False):

        interface.ReplFixture.__init__(self, logger, job_num)

        if "dbpath" in mongod_options:
            raise ValueError("Cannot specify mongod_options.dbpath")

        self.mongod_executable = mongod_executable
        self.mongod_options = utils.default_if_none(mongod_options, {})
        self.main_options = utils.default_if_none(main_options, {})
        self.subordinate_options = utils.default_if_none(subordinate_options, {})
        self.preserve_dbpath = preserve_dbpath

        # Command line options override the YAML configuration.
        dbpath_prefix = utils.default_if_none(config.DBPATH_PREFIX, dbpath_prefix)
        dbpath_prefix = utils.default_if_none(dbpath_prefix, config.DEFAULT_DBPATH_PREFIX)
        self._dbpath_prefix = os.path.join(dbpath_prefix,
                                           "job{}".format(self.job_num),
                                           config.FIXTURE_SUBDIR)

        self.main = None
        self.subordinate = None

    def setup(self):
        if self.main is None:
            self.main = self._new_mongod_main()
        self.main.setup()

        if self.subordinate is None:
            self.subordinate = self._new_mongod_subordinate()
        self.subordinate.setup()

    def await_ready(self):
        self.main.await_ready()
        self.subordinate.await_ready()

        # Do a replicated write to ensure that the subordinate has finished with its initial sync before
        # starting to run any tests.
        client = self.main.mongo_client()

        # Keep retrying this until it times out waiting for replication.
        def insert_fn(remaining_secs):
            remaining_millis = int(round(remaining_secs * 1000))
            write_concern = pymongo.WriteConcern(w=2, wtimeout=remaining_millis)
            coll = client.resmoke.get_collection("await_ready", write_concern=write_concern)
            coll.insert_one({"awaiting": "ready"})

        try:
            self.retry_until_wtimeout(insert_fn)
        except pymongo.errors.WTimeoutError:
            self.logger.info("Replication of write operation timed out.")
            raise

    def _do_teardown(self):
        running_at_start = self.is_running()
        success = True  # Still a success if nothing is running.

        if not running_at_start:
            self.logger.info(
                "Main-subordinate deployment was expected to be running in _do_teardown(), but wasn't.")

        if self.subordinate is not None:
            if running_at_start:
                self.logger.info("Stopping subordinate...")

            success = self.subordinate.teardown()

            if running_at_start:
                self.logger.info("Successfully stopped subordinate.")

        if self.main is not None:
            if running_at_start:
                self.logger.info("Stopping main...")

            success = self.main.teardown() and success

            if running_at_start:
                self.logger.info("Successfully stopped main.")

        return success

    def is_running(self):
        return (self.main is not None and self.main.is_running() and
                self.subordinate is not None and self.subordinate.is_running())

    def get_primary(self):
        return self.main

    def get_secondaries(self):
        return [self.subordinate]

    def _new_mongod(self, mongod_logger, mongod_options):
        """
        Returns a standalone.MongoDFixture with the specified logger and
        options.
        """
        return standalone.MongoDFixture(mongod_logger,
                                        self.job_num,
                                        mongod_executable=self.mongod_executable,
                                        mongod_options=mongod_options,
                                        preserve_dbpath=self.preserve_dbpath)

    def _new_mongod_main(self):
        """
        Returns a standalone.MongoDFixture configured to be used as the
        main of a main-subordinate deployment.
        """

        mongod_logger = self.logger.new_fixture_node_logger("main")

        mongod_options = self.mongod_options.copy()
        mongod_options.update(self.main_options)
        mongod_options["main"] = ""
        mongod_options["dbpath"] = os.path.join(self._dbpath_prefix, "main")
        return self._new_mongod(mongod_logger, mongod_options)

    def _new_mongod_subordinate(self):
        """
        Returns a standalone.MongoDFixture configured to be used as the
        subordinate of a main-subordinate deployment.
        """

        mongod_logger = self.logger.new_fixture_node_logger("subordinate")

        mongod_options = self.mongod_options.copy()
        mongod_options.update(self.subordinate_options)
        mongod_options["subordinate"] = ""
        mongod_options["source"] = self.main.get_internal_connection_string()
        mongod_options["dbpath"] = os.path.join(self._dbpath_prefix, "subordinate")
        return self._new_mongod(mongod_logger, mongod_options)

    def get_internal_connection_string(self):
        return self.main.get_internal_connection_string()

    def get_driver_connection_url(self):
        return self.main.get_driver_connection_url()
