/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

// Copyright (c) 2017 Copyright Holder All Rights Reserved.
#pragma once

#include <quic/state/StateData.h>

namespace quic {

// Common operations to conduct on QuicStreamState when send reset on it
void resetQuicStream(QuicStreamState& stream, ApplicationErrorCode error);

// Comon operations to conduct on QuicStreamState when receive reset on it
void onResetQuicStream(QuicStreamState& stream, RstStreamFrame&& frame);

bool isAllDataReceived(const QuicStreamState& stream);
}