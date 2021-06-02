#include "Node.h"

void Node::ApplyStyle(Ref<StyleGroup> stylegroup)
{
	for (auto& s : stylegroup->GetProps())
	{
		switch (s.first)
		{
		case PropType::HEIGHT:
		{
			Height.Unit = (Layout::Unit)(s.second.type);
			float value = s.second.value.Number;
			Height.Value = value;
		}
			break;
		case PropType::MIN_HEIGHT:
			MinHeight.Unit = (Layout::Unit)s.second.type;
			MinHeight.Value = s.second.value.Number;
			break;
		case PropType::MAX_HEIGHT:
			MaxHeight.Unit = (Layout::Unit)s.second.type;
			MaxHeight.Value = s.second.value.Number;
			break;
		case PropType::WIDTH:
			Width.Unit = (Layout::Unit)s.second.type;
			Width.Value = s.second.value.Number;
			break;
		case PropType::MIN_WIDTH:
			MinWidth.Unit = (Layout::Unit)s.second.type;
			MinWidth.Value = s.second.value.Number;
			break;
		case PropType::MAX_WIDTH:
			MaxWidth.Unit = (Layout::Unit)s.second.type;
			MaxWidth.Value = s.second.value.Number;
			break;
		case PropType::MARGIN_RIGHT:
			Margin.Right.Unit = (Layout::Unit)s.second.type;
			Margin.Right.Value = s.second.value.Number;
			break;
		case PropType::MARGIN_LEFT:
			Margin.Left.Unit = (Layout::Unit)s.second.type;
			Margin.Left.Value = s.second.value.Number;
			break;
		case PropType::MARGIN_TOP:
			Margin.Top.Unit = (Layout::Unit)s.second.type;
			Margin.Top.Value = s.second.value.Number;
			break;
		case PropType::MARGIN_BOTTOM:
			Margin.Bottom.Unit = (Layout::Unit)s.second.type;
			Margin.Bottom.Value = s.second.value.Number;
			break;
		}
	}
}

Node::Node()
{
	Childrens = std::vector<Ref<Node>>();
	Groups = std::vector<std::string>();

	PositionType = Layout::PositionType::RELATIVE;
	Position = Layout::LayoutVec4();
	Height = MaxHeight = MinHeight = Layout::LayoutUnit();
	Margin = Padding = Border = Layout::LayoutVec4();
	Direction = Layout::LayoutDirection::LTR;
	FlexDirection = Layout::FlexDirection::ROW;
	FlexWrap = Layout::FlexWrap::WRAP;
	FlexGrow = 0.f;
	FlexShrink = 1.f;
	FlexBasis = 10.f;
	AspectRatio = 1.f;
	AlignItems = Layout::AlignItems::FLEX_START;
	SelfAlign = Layout::AlignItems::FLEX_START;
}