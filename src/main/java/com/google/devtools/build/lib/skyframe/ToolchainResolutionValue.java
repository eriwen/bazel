// Copyright 2017 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.devtools.build.lib.skyframe;

import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.devtools.build.lib.analysis.config.BuildConfiguration;
import com.google.devtools.build.lib.analysis.platform.PlatformInfo;
import com.google.devtools.build.lib.cmdline.Label;
import com.google.devtools.build.skyframe.SkyFunctionName;
import com.google.devtools.build.skyframe.SkyKey;
import com.google.devtools.build.skyframe.SkyValue;
import java.util.List;

/**
 * A value which represents the map of potential execution platforms and resolved toolchains. This
 * value only considers a single toolchain type, which allows for a Skyframe cache per toolchain
 * type. Callers will need to consider all toolchain types that are required and merge the results
 * together appropriately.
 */
@AutoValue
public abstract class ToolchainResolutionValue implements SkyValue {

  // A key representing the input data.
  public static SkyKey key(
      BuildConfiguration configuration,
      Label toolchainType,
      PlatformInfo targetPlatform,
      List<PlatformInfo> availableExecutionPlatforms) {
    return ToolchainResolutionKey.create(
        configuration, toolchainType, targetPlatform, availableExecutionPlatforms);
  }

  /** {@link SkyKey} implementation used for {@link ToolchainResolutionFunction}. */
  @AutoValue
  public abstract static class ToolchainResolutionKey implements SkyKey {
    @Override
    public SkyFunctionName functionName() {
      return SkyFunctions.TOOLCHAIN_RESOLUTION;
    }

    public abstract BuildConfiguration configuration();

    public abstract Label toolchainType();

    public abstract PlatformInfo targetPlatform();

    public abstract ImmutableList<PlatformInfo> availableExecutionPlatforms();

    public static ToolchainResolutionKey create(
        BuildConfiguration configuration,
        Label toolchainType,
        PlatformInfo targetPlatform,
        List<PlatformInfo> availableExecutionPlatforms) {
      return new AutoValue_ToolchainResolutionValue_ToolchainResolutionKey(
          configuration,
          toolchainType,
          targetPlatform,
          ImmutableList.copyOf(availableExecutionPlatforms));
    }
  }

  public static ToolchainResolutionValue create(
      ImmutableMap<PlatformInfo, Label> availableToolchainLabels) {
    return new AutoValue_ToolchainResolutionValue(availableToolchainLabels);
  }

  /**
   * Returns the resolved set of toolchain labels (as {@link Label}) for the requested toolchain
   * type, keyed by the execution platforms (as {@link PlatformInfo}). Ordering is not preserved, if
   * the caller cares about the order of platforms it must take care of that directly.
   */
  public abstract ImmutableMap<PlatformInfo, Label> availableToolchainLabels();
}
