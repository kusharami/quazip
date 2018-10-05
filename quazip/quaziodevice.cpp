/*
Copyright (C) 2005-2014 Sergey A. Tachenov
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP.

QuaZIP is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

QuaZIP is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with QuaZIP.  If not, see <http://www.gnu.org/licenses/>.

See COPYING file for the full LGPL text.

Original ZIP package is copyrighted by Gilles Vollant and contributors,
see quazip/(un)zip.h files for details. Basically it's the zlib license.
*/

#include "quaziodevice.h"

#define QUAZIO_BUFFER_SIZE 16384

/// \cond internal
class QuaZIODevicePrivate {
public:
  QuaZIODevice *owner;
  QIODevice *io;
  qint64 ioStartPosition;
  qint64 ioPosition;
  bool atEnd;
  bool hasError;
  z_stream zstream;
  Bytef zbuffer[QUAZIO_BUFFER_SIZE];

  QuaZIODevicePrivate(QuaZIODevice *owner, QIODevice *io);
  bool flushBuffer(int size = QUAZIO_BUFFER_SIZE);
  bool seekInternal(qint64 pos);
  qint64 readInternal(char *data, qint64 maxlen);
  bool initRead();
  qint64 writeInternal(const char *data, qint64 maxlen);
  bool initWrite();
  bool seekInit();
  bool check(int code);
  void close();
  void setError(const QString &message);
};

QuaZIODevicePrivate::QuaZIODevicePrivate(QuaZIODevice *owner, QIODevice *io)
	: owner(owner), io(io), ioStartPosition(io ? io->pos() : 0), ioPosition(0),
	  atEnd(false), hasError(io == nullptr) {
  Q_ASSERT(io);
  memset(&zstream, 0, sizeof(zstream));
}

bool QuaZIODevicePrivate::flushBuffer(int size) {
  if (io->write(reinterpret_cast<char *>(zbuffer), size) != size) {
	setError(io->errorString());
  }

  return !hasError;
}

bool QuaZIODevicePrivate::seekInternal(qint64 pos) {
  if (!owner->isOpen() || owner->isWritable())
	return false;

  if (io->isSequential())
	return true;

  qint64 currentPos = qint64(zstream.total_out);
  currentPos -= pos;
  if (currentPos > 0) {
	if (!check(inflateReset(&zstream))) {
	  return false;
	}

	ioPosition = ioStartPosition;

	zstream.next_in = zbuffer;
	zstream.avail_in = 0;
  } else {
	pos = -currentPos;
  }

  while (pos > 0) {
	char buf[4096];

	auto blockSize = qMin(pos, qint64(sizeof(buf)));

	auto readBytes = readInternal(buf, blockSize);
	if (readBytes != blockSize) {
	  return false;
	}
	pos -= readBytes;
  }

  return true;
}

qint64 QuaZIODevicePrivate::readInternal(char *data, qint64 maxlen) {
  if (!owner->isOpen() || !seekInit()) {
	return -1;
  }

  zstream.next_out = reinterpret_cast<decltype(zstream.next_out)>(data);

  qint64 count = maxlen;
  auto blockSize = std::numeric_limits<decltype(zstream.avail_out)>::max();
  bool run = true;

  while (run && count > 0) {
	if (count < blockSize) {
	  blockSize = static_cast<decltype(blockSize)>(count);
	}

	zstream.avail_out = blockSize;

	while (zstream.avail_out > 0) {
	  if (zstream.avail_in == 0) {
		auto readResult =
			io->read(reinterpret_cast<char *>(zbuffer), sizeof(zbuffer));

		zstream.avail_in =
			readResult >= 0
				? static_cast<decltype(zstream.avail_in)>(readResult)
				: 0;
		if (readResult <= 0) {
		  run = false;
		  atEnd = true;
		  if (readResult < 0) {
			setError(io->errorString());
		  }
		  break;
		}

		zstream.next_in = zbuffer;
		ioPosition += zstream.avail_in;
	  }

	  int code = inflate(&zstream, Z_NO_FLUSH);
	  if (code == Z_STREAM_END) {
		run = false;
		atEnd = true;
		break;
	  }

	  if (!check(code)) {
		run = false;
		break;
	  }
	}

	count -= blockSize - zstream.avail_out;
  }

  return maxlen - count;
}

bool QuaZIODevicePrivate::initRead() {
  if (!io->isReadable()) {
	setError("Dependent device is not readable.");
	return false;
  }

  atEnd = false;
  zstream.next_in = zbuffer;
  zstream.avail_in = 0;

  return check(inflateInit(&zstream));
}

qint64 QuaZIODevicePrivate::writeInternal(const char *data, qint64 maxlen) {
  if (!io->isOpen() || !seekInit()) {
	return -1;
  }

  qint64 count = maxlen;
  auto blockSize = std::numeric_limits<decltype(zstream.avail_in)>::max();

  zstream.next_in = reinterpret_cast<decltype(zstream.next_in)>(data);

  bool run = true;
  while (run && count > 0) {
	if (count < blockSize) {
	  blockSize = static_cast<decltype(blockSize)>(count);
	}

	zstream.avail_in = blockSize;

	while (zstream.avail_in > 0) {
	  if (!check(deflate(&zstream, Z_NO_FLUSH))) {
		run = false;
		break;
	  }
	  if (zstream.avail_out == 0) {
		if (!flushBuffer()) {
		  run = false;
		  break;
		}

		ioPosition += sizeof(zbuffer);
		zstream.next_out = zbuffer;
		zstream.avail_out = sizeof(zbuffer);
	  }
	}

	count -= blockSize - zstream.avail_in;
  }

  return maxlen - count;
}

bool QuaZIODevicePrivate::seekInit() {
  if (io->isTextModeEnabled() ||
	  (!io->isSequential() && !io->seek(ioPosition))) {
	setError("Dependent device seek failed.");
	return false;
  }

  return true;
}

bool QuaZIODevicePrivate::check(int code) {
  if (code < 0) {
	setError(QLatin1String(zstream.msg));
	return false;
  }

  return true;
}

bool QuaZIODevicePrivate::initWrite() {
  atEnd = true;

  if (!io->isWritable()) {
	setError("Dependent device is not writable.");
	return false;
  }

  zstream.next_out = zbuffer;
  zstream.avail_out = sizeof(zbuffer);

  return check(deflateInit(&zstream, Z_DEFAULT_COMPRESSION));
}

void QuaZIODevicePrivate::close() {
  auto openMode = owner->openMode();

  if ((openMode & QIODevice::ReadOnly) != 0) {
	ioPosition -= zstream.avail_in;
	seekInit();
	check(inflateEnd(&zstream));
  } else if ((openMode & QIODevice::WriteOnly) != 0) {
	zstream.next_in = Z_NULL;
	zstream.avail_in = 0;
	if (seekInit()) {
	  while (true) {
		int result = deflate(&zstream, Z_FINISH);
		if (!check(result))
		  break;

		if (result == Z_STREAM_END)
		  break;

		Q_ASSERT(zstream.avail_out == 0);

		if (!flushBuffer())
		  break;

		zstream.next_out = zbuffer;
		zstream.avail_out = sizeof(zbuffer);
	  }
	}

	if (!hasError && zstream.avail_out < sizeof(zbuffer)) {
	  flushBuffer(sizeof(zbuffer) - zstream.avail_out);
	}
	check(deflateEnd(&zstream));
  }
}

void QuaZIODevicePrivate::setError(const QString &message) {
  hasError = true;
  owner->setErrorString(message);
}

/// \endcond

QuaZIODevice::QuaZIODevice(QIODevice *io, QObject *parent)
	: QIODevice(parent), d(new QuaZIODevicePrivate(this, io)) {
  connect(io, &QIODevice::readyRead, this, &QuaZIODevice::readyRead);
  connect(io, &QIODevice::aboutToClose, this, &QuaZIODevice::close);
}

QuaZIODevice::~QuaZIODevice() {
  close();
  delete d;
}

QIODevice *QuaZIODevice::getIoDevice() const { return d->io; }

bool QuaZIODevice::atEnd() const {
  if (openMode() & ReadOnly)
	return d->atEnd;

  return true;
}

bool QuaZIODevice::open(QIODevice::OpenMode mode) {
  d->hasError = false;
  if ((mode & QIODevice::ReadWrite) == QIODevice::ReadWrite) {
	d->setError("QIODevice::ReadWrite is not supported for"
				" QuaZIODevice");
	return false;
  }

  if (!d->io->isOpen()) {
	if (!d->io->open(mode & ~(Text | Unbuffered))) {
	  d->setError("Dependent device for "
				  " QuaZIODevice could not be opened.");
	  return false;
	}

	if (0 != (mode & (Append | Truncate))) {
	  d->ioStartPosition = d->io->pos();
	}
  }

  if (d->io->isTextModeEnabled()) {
	d->setError("Dependent device is not binary.");
	return false;
  }

  if ((mode & QIODevice::ReadOnly) != 0) {
	if (!d->initRead()) {
	  return false;
	}
	mode |= Unbuffered;
  }

  if ((mode & QIODevice::WriteOnly) != 0) {
	if (!d->initWrite()) {
	  return false;
	}
  }

  d->ioPosition = d->ioStartPosition;
  setErrorString(QString());
  return QIODevice::open(mode);
}

void QuaZIODevice::close() {
  if (!isOpen())
	return;

  d->close();
  QString errorString;
  if (d->hasError)
	errorString = this->errorString();
  QIODevice::close();
  setErrorString(errorString);
}

qint64 QuaZIODevice::readData(char *data, qint64 maxSize) {
  if (isReadable() && d->seekInternal(pos())) {
	return d->readInternal(data, maxSize);
  }

  return 0;
}

qint64 QuaZIODevice::writeData(const char *data, qint64 maxSize) {
  return d->writeInternal(data, maxSize);
}

bool QuaZIODevice::isSequential() const {
  if (openMode() & ReadOnly)
	return d->io->isSequential();

  return true;
}

qint64 QuaZIODevice::bytesAvailable() const {
  if (!d->atEnd && 0 != (openMode() & ReadOnly))
	return 0xFFFFFFFF;

  return 0;
}

qint64 QuaZIODevice::size() const {
  if (!isOpen())
	return 0;

  return bytesAvailable();
}

bool QuaZIODevice::hasError() const { return d->hasError; }
