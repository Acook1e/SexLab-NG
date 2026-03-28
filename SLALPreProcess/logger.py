import logging


class Logger:
    def __init__(self):
        self.logger = logging.getLogger("SLALPreProcess")
        self.logger.setLevel(logging.DEBUG)
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            "[%(asctime)s] %(levelname)s: %(message)s", "%Y-%m-%d %H:%M:%S")
        handler.setFormatter(formatter)
        self.logger.addHandler(handler)

    def debug(self, message: str, *args):
        self.logger.debug(message.format(*args))

    def info(self, message: str, *args):
        self.logger.info(message.format(*args))

    def warn(self, message: str, *args):
        self.logger.warning(message.format(*args))

    def error(self, message: str, *args):
        self.logger.error(message.format(*args))
