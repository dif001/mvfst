/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/client/handshake/ClientHandshake.h>

#include <fizz/protocol/Protocol.h>
#include <quic/handshake/FizzBridge.h>
#include <quic/state/QuicStreamFunctions.h>

namespace quic {

ClientHandshake::ClientHandshake(QuicCryptoState& cryptoState)
    : cryptoState_(cryptoState) {}

void ClientHandshake::doHandshake(
    std::unique_ptr<folly::IOBuf> data,
    EncryptionLevel encryptionLevel) {
  if (!data) {
    return;
  }
  // TODO: deal with clear text alert messages. It's possible that a MITM who
  // mucks with the finished messages could cause the decryption to be invalid
  // on the server, which would result in a cleartext close or a cleartext
  // alert. We currently switch to 1-rtt ciphers immediately for reads and
  // throw away the cleartext cipher for reads, this would result in us
  // dropping the alert and timing out instead.
  if (phase_ == Phase::Initial) {
    // This could be an HRR or a cleartext alert.
    phase_ = Phase::Handshake;
  }

  // First add it to the right read buffer.
  switch (encryptionLevel) {
    case EncryptionLevel::Initial:
      initialReadBuf_.append(std::move(data));
      break;
    case EncryptionLevel::Handshake:
      handshakeReadBuf_.append(std::move(data));
      break;
    case EncryptionLevel::EarlyData:
    case EncryptionLevel::AppData:
      appDataReadBuf_.append(std::move(data));
      break;
  }
  // Get the current buffer type the transport is accepting.
  waitForData_ = false;
  while (!waitForData_) {
    switch (getReadRecordLayerEncryptionLevel()) {
      case EncryptionLevel::Initial:
        processSocketData(initialReadBuf_);
        break;
      case EncryptionLevel::Handshake:
        processSocketData(handshakeReadBuf_);
        break;
      case EncryptionLevel::EarlyData:
      case EncryptionLevel::AppData:
        processSocketData(appDataReadBuf_);
        break;
    }
    if (error_) {
      error_.throw_exception();
    }
  }
}

std::unique_ptr<Aead> ClientHandshake::getOneRttWriteCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(oneRttWriteCipher_);
}

std::unique_ptr<Aead> ClientHandshake::getOneRttReadCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(oneRttReadCipher_);
}

std::unique_ptr<Aead> ClientHandshake::getZeroRttWriteCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(zeroRttWriteCipher_);
}

std::unique_ptr<Aead> ClientHandshake::getHandshakeReadCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(handshakeReadCipher_);
}

std::unique_ptr<Aead> ClientHandshake::getHandshakeWriteCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(handshakeWriteCipher_);
}

std::unique_ptr<PacketNumberCipher>
ClientHandshake::getOneRttReadHeaderCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(oneRttReadHeaderCipher_);
}

std::unique_ptr<PacketNumberCipher>
ClientHandshake::getOneRttWriteHeaderCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(oneRttWriteHeaderCipher_);
}

std::unique_ptr<PacketNumberCipher>
ClientHandshake::getHandshakeReadHeaderCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(handshakeReadHeaderCipher_);
}

std::unique_ptr<PacketNumberCipher>
ClientHandshake::getHandshakeWriteHeaderCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(handshakeWriteHeaderCipher_);
}

std::unique_ptr<PacketNumberCipher>
ClientHandshake::getZeroRttWriteHeaderCipher() {
  if (error_) {
    error_.throw_exception();
  }
  return std::move(zeroRttWriteHeaderCipher_);
}

/**
 * Notify the crypto layer that we received one rtt protected data.
 * This allows us to know that the peer has implicitly acked the 1-rtt keys.
 */
void ClientHandshake::onRecvOneRttProtectedData() {
  if (phase_ != Phase::Established) {
    phase_ = Phase::Established;
  }
}

ClientHandshake::Phase ClientHandshake::getPhase() const {
  return phase_;
}

folly::Optional<ServerTransportParameters>
ClientHandshake::getServerTransportParams() {
  return transportParams_->getServerTransportParams();
}

bool ClientHandshake::isTLSResumed() const {
  auto pskType = state_.pskType();
  return pskType && *pskType == fizz::PskType::Resumption;
}

folly::Optional<bool> ClientHandshake::getZeroRttRejected() {
  return std::move(zeroRttRejected_);
}

const folly::Optional<std::string>& ClientHandshake::getApplicationProtocol()
    const {
  auto& earlyDataParams = state_.earlyDataParams();
  if (earlyDataParams) {
    return earlyDataParams->alpn;
  } else {
    return state_.alpn();
  }
}

void ClientHandshake::computeCiphers(CipherKind kind, folly::ByteRange secret) {
  auto aead = buildAead(kind, secret);
  auto packetNumberCipher = cryptoFactory_->makePacketNumberCipher(secret);
  switch (kind) {
    case CipherKind::HandshakeWrite:
      handshakeWriteCipher_ = std::move(aead);
      handshakeWriteHeaderCipher_ = std::move(packetNumberCipher);
      break;
    case CipherKind::HandshakeRead:
      handshakeReadCipher_ = std::move(aead);
      handshakeReadHeaderCipher_ = std::move(packetNumberCipher);
      break;
    case CipherKind::OneRttWrite:
      oneRttWriteCipher_ = std::move(aead);
      oneRttWriteHeaderCipher_ = std::move(packetNumberCipher);
      break;
    case CipherKind::OneRttRead:
      oneRttReadCipher_ = std::move(aead);
      oneRttReadHeaderCipher_ = std::move(packetNumberCipher);
      break;
    case CipherKind::ZeroRttWrite:
      zeroRttWriteCipher_ = std::move(aead);
      zeroRttWriteHeaderCipher_ = std::move(packetNumberCipher);
      break;
    default:
      // Report error?
      break;
  }
}

void ClientHandshake::raiseError(folly::exception_wrapper error) {
  error_ = std::move(error);
}

void ClientHandshake::waitForData() {
  waitForData_ = true;
}

EncryptionLevel ClientHandshake::getReadRecordLayerEncryptionLevel() {
  return getEncryptionLevelFromFizz(
      state_.readRecordLayer()->getEncryptionLevel());
}

std::unique_ptr<Aead> ClientHandshake::buildAead(
    CipherKind kind,
    folly::ByteRange secret) {
  bool isEarlyTraffic = kind == CipherKind::ZeroRttWrite;
  fizz::CipherSuite cipher =
      isEarlyTraffic ? state_.earlyDataParams()->cipher : *state_.cipher();
  std::unique_ptr<fizz::KeyScheduler> keySchedulerPtr = isEarlyTraffic
      ? state_.context()->getFactory()->makeKeyScheduler(cipher)
      : nullptr;
  fizz::KeyScheduler& keyScheduler =
      isEarlyTraffic ? *keySchedulerPtr : *state_.keyScheduler();

  return FizzAead::wrap(fizz::Protocol::deriveRecordAeadWithLabel(
      *state_.context()->getFactory(),
      keyScheduler,
      cipher,
      secret,
      kQuicKeyLabel,
      kQuicIVLabel));
}

void ClientHandshake::writeDataToStream(
    EncryptionLevel encryptionLevel,
    Buf data) {
  if (encryptionLevel == EncryptionLevel::AppData) {
    // Don't write 1-rtt handshake data on the client.
    return;
  }
  auto cryptoStream = getCryptoStream(cryptoState_, encryptionLevel);
  writeDataToQuicStream(*cryptoStream, std::move(data));
}

void ClientHandshake::computeZeroRttCipher() {
  VLOG(10) << "Computing Client zero rtt keys";
  CHECK(state_.earlyDataParams().hasValue());
  earlyDataAttempted_ = true;
}

void ClientHandshake::computeOneRttCipher(bool earlyDataAccepted) {
  // The 1-rtt handshake should have succeeded if we know that the early
  // write failed. We currently treat the data as lost.
  // TODO: we need to deal with HRR based rejection as well, however we don't
  // have an API right now.
  if (earlyDataAttempted_ && !earlyDataAccepted) {
    if (fizz::client::earlyParametersMatch(state_)) {
      zeroRttRejected_ = true;
    } else {
      // TODO: support app retry of zero rtt data.
      error_ = folly::make_exception_wrapper<QuicInternalException>(
          "Changing parameters when early data attempted not supported",
          LocalErrorCode::EARLY_DATA_REJECTED);
      return;
    }
  }
  // After a successful handshake we should send packets with the type of
  // ClientCleartext. We assume that by the time we get the data for the QUIC
  // stream, the server would have also acked all the client initial packets.
  phase_ = Phase::OneRttKeysDerived;
}

} // namespace quic
