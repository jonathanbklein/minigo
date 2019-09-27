// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/dual_net/dual_net.h"

#include <array>
#include <deque>
#include <map>
#include <vector>

#include "cc/position.h"
#include "cc/random.h"
#include "cc/symmetries.h"
#include "cc/test_utils.h"
#include "gtest/gtest.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_def_builder.h"

#if MG_ENABLE_TF_DUAL_NET
#include "cc/dual_net/tf_dual_net.h"
#endif
#if MG_ENABLE_LITE_DUAL_NET
#include "cc/dual_net/lite_dual_net.h"
#endif

namespace minigo {
namespace {

struct AgzFeatures {
  static constexpr Model::FeatureType kFeatureType = Model::FeatureType::kAgz;
  static constexpr int kNumFeaturePlanes = 17;
};

struct ExtraFeatures {
  static constexpr Model::FeatureType kFeatureType = Model::FeatureType::kExtra;
  static constexpr int kNumFeaturePlanes = 20;
};

template <typename FeatureType>
class DualNetTest : public ::testing::Test {
 public:
  static constexpr int kNumFeaturePlanes = FeatureType::kNumFeaturePlanes;
  static constexpr Model::FeatureType kFeatureType = FeatureType::kFeatureType;

  std::vector<float> GetStoneFeatures(const Model::Tensor<float>& features,
                                      Coord c) {
    std::vector<float> result;
    MG_CHECK(features.n == 1);
    MG_CHECK(features.c == kNumFeaturePlanes);
    for (int i = 0; i < kNumFeaturePlanes; ++i) {
      result.push_back(features.data[c * kNumFeaturePlanes + i]);
    }
    return result;
  }
};

using FeatureTypes = ::testing::Types<AgzFeatures, ExtraFeatures>;
TYPED_TEST_CASE(DualNetTest, FeatureTypes);

// Verifies SetFeatures an empty board with black to play.
TYPED_TEST(DualNetTest, TestEmptyBoardBlackToPlay) {
  TestablePosition board("");
  Model::Input input;
  input.sym = symmetry::kIdentity;
  input.position_history.push_back(&board);

  DualNet::BoardFeatureBuffer<float> buffer;
  Model::Tensor<float> features = {1, kN, kN, this->kNumFeaturePlanes, buffer.data()};
  DualNet::SetFeatures({&input}, this->kFeatureType, &features);

  for (int c = 0; c < kN * kN; ++c) {
    auto f = this->GetStoneFeatures(features, c);
    for (size_t i = 0; i < f.size(); ++i) {
      if (i != DualNet::kPlayerFeature) {
        EXPECT_EQ(0, f[i]);
      } else {
        EXPECT_EQ(1, f[i]);
      }
    }
  }
}

// Verifies SetFeatures for an empty board with white to play.
TYPED_TEST(DualNetTest, TestEmptyBoardWhiteToPlay) {
  TestablePosition board("", Color::kWhite);
  Model::Input input;
  input.sym = symmetry::kIdentity;
  input.position_history.push_back(&board);

  DualNet::BoardFeatureBuffer<float> buffer;
  Model::Tensor<float> features = {1, kN, kN, this->kNumFeaturePlanes, buffer.data()};
  DualNet::SetFeatures({&input}, this->kFeatureType, &features);

  for (int c = 0; c < kN * kN; ++c) {
    auto f = this->GetStoneFeatures(features, c);
    for (size_t i = 0; i < f.size(); ++i) {
      EXPECT_EQ(0, f[i]);
    }
  }
}

// Verifies SetFeatures.
TYPED_TEST(DualNetTest, TestSetFeatures) {
  TestablePosition board("");

  std::vector<std::string> moves = {"B9", "H9", "A8", "J9",
                                    "D5", "A1", "A2", "J1"};
  std::deque<TestablePosition> positions;
  for (const auto& move : moves) {
    board.PlayMove(move);
    positions.push_front(board);
  }

  Model::Input input;
  input.sym = symmetry::kIdentity;
  for (const auto& p : positions) {
    input.position_history.push_back(&p);
  }

  DualNet::BoardFeatureBuffer<float> buffer;
  Model::Tensor<float> features = {1, kN, kN, this->kNumFeaturePlanes, buffer.data()};
  DualNet::SetFeatures({&input}, this->kFeatureType, &features);

  //                        B0 W0 B1 W1 B2 W2 B3 W3 B4 W4 B5 W5 B6 W6 B7 W7 C
  std::vector<float> b9 = {{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}};
  std::vector<float> h9 = {{0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1}};
  std::vector<float> a8 = {{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1}};
  std::vector<float> j9 = {{0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1}};
  std::vector<float> d5 = {{1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  std::vector<float> a1 = {{0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  std::vector<float> a2 = {{1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  std::vector<float> j1 = {{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};

  MG_LOG(INFO) << input.position_history[0]->ToPrettyString();
  if (this->kFeatureType == Model::FeatureType::kExtra) {
    //                   L1 L2 L3
    b9.insert(b9.end(), {0, 0, 1});  // 3 liberties
    h9.insert(h9.end(), {0, 0, 1});  // 3 liberties
    a8.insert(a8.end(), {0, 0, 1});  // 3 liberties
    j9.insert(j9.end(), {0, 0, 1});  // 3 liberties
    d5.insert(d5.end(), {0, 0, 0});  // 4 liberties
    a1.insert(a1.end(), {1, 0, 0});  // 1 liberty
    a2.insert(a2.end(), {0, 1, 0});  // 2 liberties
    j1.insert(j1.end(), {0, 1, 0});  // 2 liberties
  }

  EXPECT_EQ(b9, this->GetStoneFeatures(features, Coord::FromString("B9")));
  EXPECT_EQ(h9, this->GetStoneFeatures(features, Coord::FromString("H9")));
  EXPECT_EQ(a8, this->GetStoneFeatures(features, Coord::FromString("A8")));
  EXPECT_EQ(j9, this->GetStoneFeatures(features, Coord::FromString("J9")));
  EXPECT_EQ(d5, this->GetStoneFeatures(features, Coord::FromString("D5")));
  EXPECT_EQ(a1, this->GetStoneFeatures(features, Coord::FromString("A1")));
  EXPECT_EQ(a2, this->GetStoneFeatures(features, Coord::FromString("A2")));
  EXPECT_EQ(j1, this->GetStoneFeatures(features, Coord::FromString("J1")));
}

// Verfies that features work as expected when capturing.
TYPED_TEST(DualNetTest, TestStoneFeaturesWithCapture) {
  TestablePosition board("");

  std::vector<std::string> moves = {"J3", "pass", "H2", "J2",
                                    "J1", "pass", "J2"};
  std::deque<TestablePosition> positions;
  for (const auto& move : moves) {
    board.PlayMove(move);
    positions.push_front(board);
  }

  Model::Input input;
  input.sym = symmetry::kIdentity;
  for (const auto& p : positions) {
    input.position_history.push_back(&p);
  }


  DualNet::BoardFeatureBuffer<float> buffer;
  Model::Tensor<float> features = {1, kN, kN, this->kNumFeaturePlanes, buffer.data()};
  DualNet::SetFeatures({&input}, this->kFeatureType, &features);

  //                        W0 B0 W1 B1 W2 B2 W3 B3 W4 B4 W5 B5 W6 B6 W7 B7 C
  std::vector<float> j2 = {{0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  if (this->kFeatureType == Model::FeatureType::kExtra) {
    //                   L1 L2 L3
    j2.insert(j2.end(), {0, 0, 0});
  }
  EXPECT_EQ(j2, this->GetStoneFeatures(features, Coord::FromString("J2")));
}

// Checks that the different backends produce the same result.
TYPED_TEST(DualNetTest, TestBackendsEqual) {
  if (this->kFeatureType != Model::FeatureType::kAgz) {
    // TODO(tommadams): generate models for other feature types.
    return;
  }

  struct Test {
    Test(std::unique_ptr<ModelFactory> factory, std::string basename)
        : factory(std::move(factory)), basename(std::move(basename)) {}
    std::unique_ptr<ModelFactory> factory;
    std::string basename;
  };

  std::map<std::string, Test> tests;
#if MG_ENABLE_TF_DUAL_NET
  tests.emplace("TfDualNet",
                Test(absl::make_unique<TfDualNetFactory>(std::vector<int>()),
                     "test_model.pb"));
#endif
#if MG_ENABLE_LITE_DUAL_NET
  tests.emplace("LiteDualNet", Test(absl::make_unique<LiteDualNetFactory>(),
                                    "test_model.tflite"));
#endif

  Random rnd(Random::kUniqueSeed, Random::kUniqueStream);
  Model::Input input;
  input.sym = symmetry::kIdentity;
  TestablePosition position("");
  for (int i =0; i < kN * kN; ++i) {
    auto c = GetRandomLegalMove(position, &rnd);
    position.PlayMove(c);
  }
  input.position_history.push_back(&position);

  Model::Output ref_output;
  std::string ref_name;

  auto policy_string = [](const std::array<float, kNumMoves>& policy) {
    std::ostringstream oss;
    std::copy(policy.begin(), policy.end(),
              std::ostream_iterator<float>(oss, " "));
    return oss.str();
  };

  for (const auto& kv : tests) {
    const auto& name = kv.first;
    auto& test = kv.second;
    MG_LOG(INFO) << "Running " << name;

    auto model =
        test.factory->NewModel(absl::StrCat("cc/dual_net/", test.basename));

    Model::Output output;
    std::vector<const Model::Input*> inputs = {&input};
    std::vector<Model::Output*> outputs = {&output};
    model->RunMany(inputs, &outputs, nullptr);

    if (ref_name.empty()) {
      ref_output = output;
      ref_name = name;
      continue;
    }

    auto pred = [](float left, float right) {
      return std::abs(left - right) <
             0.0001f * (1.0f + std::abs(left) + std::abs(right));
    };
    EXPECT_EQ(std::equal(output.policy.begin(), output.policy.end(),
                         ref_output.policy.begin(), pred),
              true)
        << name << ": " << policy_string(output.policy) << "\n"
        << ref_name << ": " << policy_string(ref_output.policy);
    EXPECT_NEAR(output.value, ref_output.value, 0.0001f)
        << name << " vs " << ref_name;
  }
}

}  // namespace
}  // namespace minigo
