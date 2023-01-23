// Copyright 2022 Robotec.AI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <graph/NodesRos2.hpp>
#include <RGLFields.hpp>

rclcpp::Node::SharedPtr Ros2PublishPointsNode::ros2Node = nullptr;
std::string Ros2PublishPointsNode::ros2NodeName = "RobotecGPULidar";
std::set<std::string> Ros2PublishPointsNode::ros2TopicNames = {};
rclcpp::executors::StaticSingleThreadedExecutor::SharedPtr
	Ros2PublishPointsNode::ros2Executor = nullptr;

void Ros2PublishPointsNode::setParameters(
	const char* topicName, const char* frameId,
	rgl_qos_policy_reliability_t qosReliability,
	rgl_qos_policy_durability_t qosDurability,
	rgl_qos_policy_history_t qosHistory, int32_t qosHistoryDepth)
{
	if (ros2Node.get() == nullptr) {
		static const char *args[] = { "--ros-args", "--disable-external-lib-logs" };
		rclcpp::init(2, args);

		ros2Node = std::make_shared<rclcpp::Node>(ros2NodeName);
		ros2Executor = std::make_shared<rclcpp::executors::StaticSingleThreadedExecutor>();
		ros2Executor->add_node(ros2Node);
	}

	if (ros2TopicNames.contains(topicName) && this->topicName != topicName) {
		throw InvalidAPIArgument("ROS2 publisher with the same topic name already exist!");
	}

	if (ros2Publisher.get() != nullptr) {
		ros2TopicNames.erase(this->topicName);
		ros2Publisher.reset();
	}

	this->topicName = topicName;
	this->frameId = frameId;
	ros2TopicNames.insert(topicName);

	rclcpp::QoS qos = rclcpp::QoS(qosHistoryDepth);
	qos.reliability(static_cast<rmw_qos_reliability_policy_t>(qosReliability));
	qos.durability(static_cast<rmw_qos_durability_policy_t>(qosDurability));
	qos.history(static_cast<rmw_qos_history_policy_t>(qosHistory));

	ros2Publisher = ros2Node->create_publisher<sensor_msgs::msg::PointCloud2>(topicName, qos);
}

void Ros2PublishPointsNode::validate()
{
	input = getValidInput<FormatPointsNode>();
	if (input->getHeight() != 1) {
		throw InvalidPipeline("ROS2 publish support unorganized pointclouds only");
	}
	updateRos2Message(input->getRequiredFieldList(), input->isDense());
}

void Ros2PublishPointsNode::schedule(cudaStream_t stream)
{
	auto fieldData = input->getFieldData(RGL_FIELD_DYNAMIC_FORMAT, stream);
	int count = input->getPointCount();
	unsigned char* charBuf = (unsigned char*)fieldData->getReadPtr(MemLoc::Host);
	std::vector<unsigned char> v(charBuf, charBuf + ros2Message.point_step * count);
	ros2Message.data = std::move(v);
	ros2Message.width = count;
	ros2Message.row_step = ros2Message.point_step * ros2Message.width;
	ros2Publisher->publish(ros2Message);
	ros2Executor->spin_some();
}

Ros2PublishPointsNode::~Ros2PublishPointsNode()
{
	ros2TopicNames.erase(topicName);
	ros2Publisher.reset();

	if (ros2TopicNames.empty()) {
		rclcpp::shutdown();
		ros2Executor.reset();
		ros2Node.reset();
	}
}

void Ros2PublishPointsNode::updateRos2Message(const std::vector<rgl_field_t>& fields, bool isDense)
{
	ros2Message = sensor_msgs::msg::PointCloud2();
	int offset = 0;
	for (const auto& field : fields) {
		auto ros2fields = toRos2Fields(field);
		auto ros2names = toRos2Names(field);
		auto ros2sizes = toRos2Sizes(field);

		for (int i = 0; i < ros2sizes.size(); ++i) {
			if (ros2fields.size() > i && ros2names.size() > i) {
				ros2Message.fields.push_back([&] {
					auto ret = sensor_msgs::msg::PointField();
					ret.name = ros2names[i];
					ret.datatype = ros2fields[i];
					ret.offset = offset;
					ret.count = 1;
					return ret;
				}());
			}
			offset += ros2sizes[i];
		}
	}
	ros2Message.height = 1,
	ros2Message.point_step = offset,
	ros2Message.is_dense = isDense,
	ros2Message.is_bigendian = false,
	ros2Message.header.frame_id = frameId;
}