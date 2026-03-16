"""
Generate FNIS behavior XML from a list file, replicating the exact structure
"""

import sys
import re
import locale
import os
import subprocess
from unittest import result
import xml.etree.ElementTree as ET
from xml.dom import minidom
from pathlib import Path

# ===== 固定数据 ==============================================================
FIXED_EVENTS = [
    "IdleForceDefaultState",
    "AnimObjectUnequip",
    "AnimObjLoad",
    "AnimObjDraw",
    "HeadTrackingOff",
    "HeadTrackingOn",
    "StartAnimatedCamera",
    "EndAnimatedCamera",
    "IdleChairSitting",
    "IdleChairGetUp",
    "FNISreserve1"
]

# 固定对象引用（模板中的名称）
REF_BLEND_TRANSITION = "#0200"
REF_CAMERA_PAYLOAD = "#0201"      # 虽然未使用，但保留
REF_GLOBAL_HEADTRACK_OFF = "#0202"    # 存在但不被状态使用
REF_GLOBAL_HEADTRACK_ON = "#0203"
REF_EMPTY_TRANS_ARRAY = "#0301"
REF_ROOT_STATEMACHINE = "#0056"
REF_BEHAVIOR_GRAPH = "#0055"
REF_ROOT_CONTAINER = "#0051"

# Havok类签名（与模板一致）
SIGNATURES = {
    "hkbBehaviorGraphStringData": "0xc713064e",
    "hkbVariableValueSet": "0x27812d8d",
    "hkbBehaviorGraphData": "0x95aca5d",
    "hkbBlendingTransitionEffect": "0xfd8584fe",
    "hkbStringEventPayload": "0xed04256a",
    "hkbStateMachineEventPropertyArray": "0xb07b4388",
    "hkbStateMachineStateInfo": "0xed7f9d0",
    "hkbClipGenerator": "0x333b85b9",
    "hkbStateMachine": "0x816c1dcb",
    "hkbBehaviorGraph": "0xb1218f86",
    "hkRootLevelContainer": "0x2772c11e",
    "hkbStateMachineTransitionInfoArray": "0xe397b11e",
}

# ===== 辅助函数 ==============================================================


def create_hkobject(name, class_name, signature=None):
    elem = ET.Element("hkobject")
    elem.set("name", name)
    elem.set("class", class_name)
    if signature:
        elem.set("signature", signature)
    return elem


def create_hkparam(name, numelements=None, text=None):
    elem = ET.Element("hkparam")
    elem.set("name", name)
    if numelements is not None:
        elem.set("numelements", str(numelements))
    if text is not None:
        elem.text = str(text)
    return elem


def add_simple_hkparam(parent, name, text):
    param = ET.SubElement(parent, "hkparam", name=name)
    param.text = str(text)
    return param


def add_null_hkparam(parent, name):
    param = ET.SubElement(parent, "hkparam", name=name)
    param.text = "null"
    return param


def add_hkobject_param(parent, name, ref):
    param = ET.SubElement(parent, "hkparam", name=name)
    param.text = ref
    return param


def add_empty_hkparam(parent, name):
    param = ET.SubElement(parent, "hkparam", name=name)
    param.set("numelements", "0")
    # 无内容
    return param


def build_event_array(name, event_id, payload_ref=None):
    """构建一个包含单个事件的 hkbStateMachineEventPropertyArray"""
    elem = create_hkobject(name, "hkbStateMachineEventPropertyArray",
                           SIGNATURES["hkbStateMachineEventPropertyArray"])
    events = ET.SubElement(elem, "hkparam", name="events")
    events.set("numelements", "1")
    ev_obj = ET.SubElement(events, "hkobject")
    id_param = ET.SubElement(ev_obj, "hkparam", name="id")
    id_param.text = str(event_id)
    payload_param = ET.SubElement(ev_obj, "hkparam", name="payload")
    payload_param.text = payload_ref if payload_ref else "null"
    return elem


def build_clip_generator(name, anim_file, mod_name):
    """构建 hkbClipGenerator"""
    elem = create_hkobject(name, "hkbClipGenerator",
                           SIGNATURES["hkbClipGenerator"])
    add_null_hkparam(elem, "variableBindingSet")
    add_simple_hkparam(elem, "userData", "0")
    clip_name = f"{mod_name}_{anim_file.replace('.hkx', '')}"
    add_simple_hkparam(elem, "name", clip_name)
    anim_path = f"Animations\\{mod_name}\\{anim_file}"
    add_simple_hkparam(elem, "animationName", anim_path)
    add_null_hkparam(elem, "triggers")
    add_simple_hkparam(elem, "cropStartAmountLocalTime", "0.000000")
    add_simple_hkparam(elem, "cropEndAmountLocalTime", "0.000000")
    add_simple_hkparam(elem, "startTime", "0.000000")
    add_simple_hkparam(elem, "playbackSpeed", "1.000000")
    add_simple_hkparam(elem, "enforcedDuration", "0.000000")
    add_simple_hkparam(elem, "userControlledTimeFraction", "0.000000")
    add_simple_hkparam(elem, "animationBindingIndex", "-1")
    add_simple_hkparam(elem, "mode", "MODE_LOOPING")
    add_simple_hkparam(elem, "flags", "0")
    return elem


def build_state_info(name, state_name, state_id, generator_ref, enter_ref, exit_ref):
    """构建 hkbStateMachineStateInfo"""
    elem = create_hkobject(name, "hkbStateMachineStateInfo",
                           SIGNATURES["hkbStateMachineStateInfo"])
    add_null_hkparam(elem, "variableBindingSet")
    listeners = ET.SubElement(elem, "hkparam", name="listeners")
    listeners.set("numelements", "0")
    add_hkobject_param(elem, "enterNotifyEvents", enter_ref)
    add_hkobject_param(elem, "exitNotifyEvents", exit_ref)
    add_null_hkparam(elem, "transitions")
    add_hkobject_param(elem, "generator", generator_ref)
    add_simple_hkparam(elem, "name", state_name)
    add_simple_hkparam(elem, "stateId", str(state_id))
    add_simple_hkparam(elem, "probability", "1.000000")
    add_simple_hkparam(elem, "enable", "true")
    return elem


def build_transition_entry(event_id, to_state_id):
    """构建单个过渡条目（用于 #0300 数组）"""
    trans = ET.Element("hkobject")
    # triggerInterval
    trig = ET.SubElement(trans, "hkparam", name="triggerInterval")
    trig_obj = ET.SubElement(trig, "hkobject")
    ET.SubElement(trig_obj, "hkparam", name="enterEventId").text = "-1"
    ET.SubElement(trig_obj, "hkparam", name="exitEventId").text = "-1"
    ET.SubElement(trig_obj, "hkparam", name="enterTime").text = "0.000000"
    ET.SubElement(trig_obj, "hkparam", name="exitTime").text = "0.000000"
    # initiateInterval
    init = ET.SubElement(trans, "hkparam", name="initiateInterval")
    init_obj = ET.SubElement(init, "hkobject")
    ET.SubElement(init_obj, "hkparam", name="enterEventId").text = "-1"
    ET.SubElement(init_obj, "hkparam", name="exitEventId").text = "-1"
    ET.SubElement(init_obj, "hkparam", name="enterTime").text = "0.000000"
    ET.SubElement(init_obj, "hkparam", name="exitTime").text = "0.000000"
    # transition
    ET.SubElement(trans, "hkparam",
                  name="transition").text = REF_BLEND_TRANSITION
    # condition
    ET.SubElement(trans, "hkparam", name="condition").text = "null"
    # eventId
    ET.SubElement(trans, "hkparam", name="eventId").text = str(event_id)
    # toStateId
    ET.SubElement(trans, "hkparam", name="toStateId").text = str(to_state_id)
    # fromNestedStateId
    ET.SubElement(trans, "hkparam", name="fromNestedStateId").text = "0"
    # toNestedStateId
    ET.SubElement(trans, "hkparam", name="toNestedStateId").text = "0"
    # priority
    ET.SubElement(trans, "hkparam", name="priority").text = "0"
    # flags
    ET.SubElement(trans, "hkparam",
                  name="flags").text = "FLAG_IS_LOCAL_WILDCARD|FLAG_IS_GLOBAL_WILDCARD|FLAG_DISABLE_CONDITION"
    return trans


def parse_line(line):
    """解析输入行，返回 (event_name, anim_file)"""
    parts = line.strip().split()
    if len(parts) < 4 or parts[0] != 'b':
        return None
    # 格式：b -md event_name anim.hkx
    return parts[2], parts[3]


def read_text_with_fallback(file_path: str) -> str:
    with open(file_path, 'rb') as file:
        raw_data = file.read()

    encodings = [
        'utf-8-sig',
        locale.getpreferredencoding(False),
        'mbcs',
        'gbk',
        'cp1252',
        'latin-1',
    ]

    tried_encodings = set()
    last_error = None
    for encoding in encodings:
        normalized_encoding = (encoding or '').lower()
        if not normalized_encoding or normalized_encoding in tried_encodings:
            continue
        tried_encodings.add(normalized_encoding)
        try:
            return raw_data.decode(encoding)
        except (LookupError, UnicodeDecodeError) as error:
            last_error = error

    if last_error is not None:
        raise last_error
    raise UnicodeDecodeError('utf-8', b'', 0, 1, 'unable to decode file')

# ===== 主生成函数 ============================================================


def generate_xml(input_lines, mod_name):
    """根据输入行和模组名生成完整的 XML 根元素"""
    # 解析输入
    events = []  # 列表 of (event_name, anim_file)
    for line in input_lines:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parsed = parse_line(line)
        if parsed:
            events.append(parsed)

    num_events = len(events)
    total_events = len(FIXED_EVENTS) + num_events
    print(f"Found {num_events} events, total events = {total_events}")

    # 创建根元素 <hkpackfile>
    root = ET.Element("hkpackfile")
    root.set("classversion", "8")
    root.set("contentsversion", "hk_2010.2.0-r2")
    root.set("toplevelobject", REF_ROOT_CONTAINER)

    # 创建 <hksection name="__data__">
    section = ET.SubElement(root, "hksection")
    section.set("name", "__data__")

    # ----- 固定对象 (编号 < 1000) -----
    # #0052: hkbBehaviorGraphStringData
    obj52 = create_hkobject(
        "#0052", "hkbBehaviorGraphStringData", SIGNATURES["hkbBehaviorGraphStringData"])
    ev_names = ET.SubElement(obj52, "hkparam", name="eventNames")
    ev_names.set("numelements", str(total_events))
    for ev in FIXED_EVENTS:
        cstr = ET.SubElement(ev_names, "hkcstring")
        cstr.text = ev
    for ev_name, _ in events:
        cstr = ET.SubElement(ev_names, "hkcstring")
        cstr.text = ev_name
    # attributeNames (固定一个)
    attr_names = ET.SubElement(obj52, "hkparam", name="attributeNames")
    attr_names.set("numelements", "1")
    cstr = ET.SubElement(attr_names, "hkcstring")
    cstr.text = "AttrWM"
    # variableNames (固定两个)
    var_names = ET.SubElement(obj52, "hkparam", name="variableNames")
    var_names.set("numelements", "2")
    cstr1 = ET.SubElement(var_names, "hkcstring")
    cstr1.text = "bAnimationDriven"
    cstr2 = ET.SubElement(var_names, "hkcstring")
    cstr2.text = "IsFNIS"
    # characterPropertyNames (空)
    add_empty_hkparam(obj52, "characterPropertyNames")
    section.append(obj52)

    # #0053: hkbVariableValueSet
    obj53 = create_hkobject("#0053", "hkbVariableValueSet",
                            SIGNATURES["hkbVariableValueSet"])
    word_vals = ET.SubElement(obj53, "hkparam", name="wordVariableValues")
    word_vals.set("numelements", "2")
    for _ in range(2):
        val_obj = ET.SubElement(word_vals, "hkobject")
        ET.SubElement(val_obj, "hkparam", name="value").text = "0"
    add_empty_hkparam(obj53, "quadVariableValues")
    add_empty_hkparam(obj53, "variantVariableValues")
    section.append(obj53)

    # #0054: hkbBehaviorGraphData
    obj54 = create_hkobject("#0054", "hkbBehaviorGraphData",
                            SIGNATURES["hkbBehaviorGraphData"])
    attr_defaults = ET.SubElement(obj54, "hkparam", name="attributeDefaults")
    attr_defaults.set("numelements", "1")
    attr_defaults.text = "0.000000"
    var_infos = ET.SubElement(obj54, "hkparam", name="variableInfos")
    var_infos.set("numelements", "2")
    # 第一个 variableInfo
    vi1 = ET.SubElement(var_infos, "hkobject")
    role1 = ET.SubElement(vi1, "hkparam", name="role")
    role_obj1 = ET.SubElement(role1, "hkobject")
    ET.SubElement(role_obj1, "hkparam", name="role").text = "ROLE_DEFAULT"
    ET.SubElement(role_obj1, "hkparam", name="flags").text = "0"
    ET.SubElement(vi1, "hkparam", name="type").text = "VARIABLE_TYPE_BOOL"
    # 第二个 variableInfo
    vi2 = ET.SubElement(var_infos, "hkobject")
    role2 = ET.SubElement(vi2, "hkparam", name="role")
    role_obj2 = ET.SubElement(role2, "hkobject")
    ET.SubElement(role_obj2, "hkparam", name="role").text = "ROLE_DEFAULT"
    ET.SubElement(role_obj2, "hkparam", name="flags").text = "0"
    ET.SubElement(vi2, "hkparam", name="type").text = "VARIABLE_TYPE_INT32"
    add_empty_hkparam(obj54, "characterPropertyInfos")
    event_infos = ET.SubElement(obj54, "hkparam", name="eventInfos")
    event_infos.set("numelements", str(total_events))
    for _ in range(total_events):
        ev_info = ET.SubElement(event_infos, "hkobject")
        ET.SubElement(ev_info, "hkparam", name="flags").text = "0"
    add_empty_hkparam(obj54, "wordMinVariableValues")
    add_empty_hkparam(obj54, "wordMaxVariableValues")
    add_hkobject_param(obj54, "variableInitialValues", "#0053")
    add_hkobject_param(obj54, "stringData", "#0052")
    section.append(obj54)

    # #0200: hkbBlendingTransitionEffect
    obj200 = create_hkobject(
        REF_BLEND_TRANSITION, "hkbBlendingTransitionEffect", SIGNATURES["hkbBlendingTransitionEffect"])
    add_null_hkparam(obj200, "variableBindingSet")
    add_simple_hkparam(obj200, "userData", "0")
    add_simple_hkparam(obj200, "name", "FNIS_06sec_BlendTransition")
    add_simple_hkparam(obj200, "selfTransitionMode",
                       "SELF_TRANSITION_MODE_CONTINUE_IF_CYCLIC_BLEND_IF_ACYCLIC")
    add_simple_hkparam(obj200, "eventMode", "EVENT_MODE_PROCESS_ALL")
    add_simple_hkparam(obj200, "duration", "0.600000")
    add_simple_hkparam(obj200, "toGeneratorStartTimeFraction", "0.000000")
    add_simple_hkparam(obj200, "flags", "FLAG_IGNORE_FROM_WORLD_FROM_MODEL")
    add_simple_hkparam(obj200, "endMode", "END_MODE_NONE")
    add_simple_hkparam(obj200, "blendCurve", "BLEND_CURVE_SMOOTH")
    section.append(obj200)

    # #0201: hkbStringEventPayload (Camera3rd)
    obj201 = create_hkobject(
        "#0201", "hkbStringEventPayload", SIGNATURES["hkbStringEventPayload"])
    add_simple_hkparam(obj201, "data", "Camera3rd [Cam3]")
    section.append(obj201)

    # #0202: 全局 HeadTrackingOff 事件数组
    obj202 = build_event_array("#0202", 4, None)
    section.append(obj202)

    # #0203: 全局 HeadTrackingOn 事件数组
    obj203 = build_event_array("#0203", 5, None)
    section.append(obj203)

    # #0301: 空过渡数组
    obj301 = create_hkobject("#0301", "hkbStateMachineTransitionInfoArray",
                             SIGNATURES["hkbStateMachineTransitionInfoArray"])
    add_empty_hkparam(obj301, "transitions")
    section.append(obj301)

    # ----- 动态对象 (状态组) -----
    state_refs = []  # 按顺序存储状态信息的引用
    # 起始编号，遵循模板间隔：状态信息 n, enter数组 n+1, exit数组 n+2, clip n+6，下一状态 n+9
    base = 1000
    for idx, (ev_name, anim_file) in enumerate(events):
        n = base + idx * 9  # 当前状态信息编号
        state_ref = f"#{n}"
        enter_ref = f"#{n+1}"
        exit_ref = f"#{n+2}"
        clip_ref = f"#{n+6}"

        # 创建 enter 事件数组 (HeadTrackingOff)
        enter_array = build_event_array(enter_ref, 4, None)
        section.append(enter_array)

        # 创建 exit 事件数组 (HeadTrackingOn)
        exit_array = build_event_array(exit_ref, 5, None)
        section.append(exit_array)

        # 创建剪辑生成器
        clip = build_clip_generator(clip_ref, anim_file, mod_name)
        section.append(clip)

        # 创建状态信息
        state_info = build_state_info(
            state_ref, ev_name, idx, clip_ref, enter_ref, exit_ref)
        section.append(state_info)

        state_refs.append(state_ref)

    # ----- #0300: 通配符过渡数组 -----
    obj300 = create_hkobject("#0300", "hkbStateMachineTransitionInfoArray",
                             SIGNATURES["hkbStateMachineTransitionInfoArray"])
    trans_param = ET.SubElement(obj300, "hkparam", name="transitions")
    trans_param.set("numelements", str(num_events))
    for idx, (ev_name, _) in enumerate(events):
        event_id = 11 + idx
        trans_entry = build_transition_entry(event_id, idx)
        trans_param.append(trans_entry)
    section.append(obj300)

    # ----- #0100: FNISIdles 状态信息 -----
    obj100 = create_hkobject(
        "#0100", "hkbStateMachineStateInfo", SIGNATURES["hkbStateMachineStateInfo"])
    add_null_hkparam(obj100, "variableBindingSet")
    add_empty_hkparam(obj100, "listeners")
    add_null_hkparam(obj100, "enterNotifyEvents")
    add_null_hkparam(obj100, "exitNotifyEvents")
    add_null_hkparam(obj100, "transitions")
    add_hkobject_param(obj100, "generator", "#0101")
    add_simple_hkparam(obj100, "name", "FNISIdles")
    add_simple_hkparam(obj100, "stateId", "0")
    add_simple_hkparam(obj100, "probability", "1.000000")
    add_simple_hkparam(obj100, "enable", "true")
    section.append(obj100)

    # ----- #0101: FNISIdlesBehavior 状态机 -----
    obj101 = create_hkobject("#0101", "hkbStateMachine",
                             SIGNATURES["hkbStateMachine"])
    add_null_hkparam(obj101, "variableBindingSet")
    add_simple_hkparam(obj101, "userData", "0")
    add_simple_hkparam(obj101, "name", "FNISIdlesBehavior")
    # eventToSendWhenStateOrTransitionChanges
    event_send = ET.SubElement(
        obj101, "hkparam", name="eventToSendWhenStateOrTransitionChanges")
    event_send_obj = ET.SubElement(event_send, "hkobject")
    ET.SubElement(event_send_obj, "hkparam", name="id").text = "-1"
    ET.SubElement(event_send_obj, "hkparam", name="payload").text = "null"
    add_null_hkparam(obj101, "startStateChooser")
    add_simple_hkparam(obj101, "startStateId", "1")   # 模板中为1，保持
    add_simple_hkparam(obj101, "returnToPreviousStateEventId", "-1")
    add_simple_hkparam(obj101, "randomTransitionEventId", "-1")
    add_simple_hkparam(obj101, "transitionToNextHigherStateEventId", "-1")
    add_simple_hkparam(obj101, "transitionToNextLowerStateEventId", "-1")
    add_simple_hkparam(obj101, "syncVariableIndex", "-1")
    add_simple_hkparam(obj101, "wrapAroundStateId", "false")
    add_simple_hkparam(obj101, "maxSimultaneousTransitions", "32")
    add_simple_hkparam(obj101, "startStateMode", "START_STATE_MODE_DEFAULT")
    add_simple_hkparam(obj101, "selfTransitionMode",
                       "SELF_TRANSITION_MODE_NO_TRANSITION")
    # states 列表 - 修改：不使用 <hkobject> 包裹，直接用空格分隔的文本
    states_param = ET.SubElement(obj101, "hkparam", name="states")
    states_param.set("numelements", str(num_events))
    # 将所有引用用空格连接，前面加一个空格确保与属性分开
    states_param.text = " " + " ".join(state_refs)
    # wildcardTransitions
    add_hkobject_param(obj101, "wildcardTransitions", "#0300")
    section.append(obj101)

    # ----- #0056: FNIS_RootBehavior 状态机 -----
    obj56 = create_hkobject("#0056", "hkbStateMachine",
                            SIGNATURES["hkbStateMachine"])
    add_null_hkparam(obj56, "variableBindingSet")
    add_simple_hkparam(obj56, "userData", "0")
    add_simple_hkparam(obj56, "name", "FNIS_RootBehavior")
    event_send56 = ET.SubElement(
        obj56, "hkparam", name="eventToSendWhenStateOrTransitionChanges")
    event_send56_obj = ET.SubElement(event_send56, "hkobject")
    ET.SubElement(event_send56_obj, "hkparam", name="id").text = "-1"
    ET.SubElement(event_send56_obj, "hkparam", name="payload").text = "null"
    add_null_hkparam(obj56, "startStateChooser")
    add_simple_hkparam(obj56, "startStateId", "0")
    add_simple_hkparam(obj56, "returnToPreviousStateEventId", "-1")
    add_simple_hkparam(obj56, "randomTransitionEventId", "-1")
    add_simple_hkparam(obj56, "transitionToNextHigherStateEventId", "-1")
    add_simple_hkparam(obj56, "transitionToNextLowerStateEventId", "-1")
    add_simple_hkparam(obj56, "syncVariableIndex", "-1")
    add_simple_hkparam(obj56, "wrapAroundStateId", "false")
    add_simple_hkparam(obj56, "maxSimultaneousTransitions", "32")
    add_simple_hkparam(obj56, "startStateMode", "START_STATE_MODE_DEFAULT")
    add_simple_hkparam(obj56, "selfTransitionMode",
                       "SELF_TRANSITION_MODE_NO_TRANSITION")
    states56 = ET.SubElement(obj56, "hkparam", name="states")
    states56.set("numelements", "1")
    states56.text = " #0100"   # 单个引用，同样不加 <hkobject>
    add_hkobject_param(obj56, "wildcardTransitions", "#0301")
    section.append(obj56)

    # ----- #0055: hkbBehaviorGraph -----
    obj55 = create_hkobject("#0055", "hkbBehaviorGraph",
                            SIGNATURES["hkbBehaviorGraph"])
    add_null_hkparam(obj55, "variableBindingSet")
    add_simple_hkparam(obj55, "userData", "0")
    add_simple_hkparam(obj55, "name", "FNISBehavior.hkb")
    add_simple_hkparam(obj55, "variableMode",
                       "VARIABLE_MODE_DISCARD_WHEN_INACTIVE")
    add_hkobject_param(obj55, "rootGenerator", "#0056")
    add_hkobject_param(obj55, "data", "#0054")
    section.append(obj55)

    # ----- #0051: hkRootLevelContainer -----
    obj51 = create_hkobject("#0051", "hkRootLevelContainer",
                            SIGNATURES["hkRootLevelContainer"])
    named_variants = ET.SubElement(obj51, "hkparam", name="namedVariants")
    named_variants.set("numelements", "1")
    variant_obj = ET.SubElement(named_variants, "hkobject")
    name_param = ET.SubElement(variant_obj, "hkparam", name="name")
    name_param.text = "hkbBehaviorGraph"
    class_param = ET.SubElement(variant_obj, "hkparam", name="className")
    class_param.text = "hkbBehaviorGraph"
    variant_param = ET.SubElement(variant_obj, "hkparam", name="variant")
    variant_param.text = "#0055"
    section.append(obj51)

    return root


def _get_hkxconv_path() -> str:
    converter_path = os.path.join(os.path.dirname(__file__), "hkxconv.exe")
    if not os.path.isfile(converter_path):
        raise FileNotFoundError(
            f"找不到 hkxconv.exe: {converter_path}"
        )
    return converter_path


def _convert_xml_to_hkx(xml_path: str, hkx_path: str) -> None:
    converter_path = _get_hkxconv_path()
    process = subprocess.run(
        [converter_path, "convert", "-v", "hkx", xml_path, hkx_path],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if process.returncode == 0:
        os.remove(xml_path)  # 转换成功后删除临时 XML 文件
    else:
        logs = []
        if process.stdout:
            logs.append(process.stdout.strip())
        if process.stderr:
            logs.append(process.stderr.strip())
        log_text = "\n".join(logs)
        raise RuntimeError(
            f"hkxconv 转换失败 (exit={process.returncode})\n{log_text}".strip()
        )


def generate_behavior(cmd_lines: list[str], output_file: Path, mod_name: str):

    root = generate_xml(cmd_lines, mod_name)

    parts = list(output_file.parts)
    for i in range(len(parts) - 2, -1, -1):
        if parts[i] == "animations":
            parts = parts[:i] + ["behaviors"] + \
                [parts[-1].replace("List", "Behavior")]
            break
    output_file = Path(*parts)

    # 转换为漂亮的 XML 字符串
    rough_string = ET.tostring(root, encoding='unicode')
    reparsed = minidom.parseString(rough_string)
    pretty_xml = reparsed.toprettyxml(indent="\t")

    # 移除 minidom 添加的空行（可选）
    pretty_xml = '\n'.join([line for line in pretty_xml.split(
        '\n') if line.strip() or line.startswith('<?')])

    output_no_ext, output_ext = os.path.splitext(str(output_file))
    xml_cache_path = output_no_ext + ".xml"
    hkx_output_path = str(output_file) if output_ext.lower(
    ) == ".hkx" else output_no_ext + ".hkx"

    os.makedirs(os.path.dirname(xml_cache_path), exist_ok=True)

    # 输出缓存到 data 并转为 hkx
    with open(xml_cache_path, 'w+', encoding='utf-8') as f:
        f.write(pretty_xml)

    _convert_xml_to_hkx(xml_cache_path, hkx_output_path)
    print(f"Generated behavior for '{mod_name}' -> {hkx_output_path}")


def process_fnis_txt(file_path: str, output_path: Path):
    results = []
    anim_stages = {}
    file_content = read_text_with_fallback(file_path)
    for line in file_content.splitlines():
        if ".hkx" not in line.lower():
            continue

        parts = line.strip().split()
        hkx_index = next(
            (index for index, part in enumerate(parts)
             if part.lower().endswith('.hkx')),
            -1
        )
        if hkx_index < 1:
            continue

        event = parts[hkx_index - 1]
        anim = parts[hkx_index]
        objs = parts[hkx_index + 1:]

        # event = eventname_An_Sn
        # anim = animname_An_Sn.hkx
        event_match = re.match(
            r"^(?P<scene>.+)_A\d+_S(?P<stage>\d+)$", event)
        anim_match = re.match(
            r"^(?P<scene>.+)_A\d+_S(?P<stage>\d+)\.hkx$",
            anim,
            re.IGNORECASE
        )
        if event_match and anim_match:
            scene_name = event_match.group('scene')
            stage_number = int(anim_match.group('stage'))
            anim_stages[scene_name] = max(
                stage_number,
                anim_stages.get(scene_name, 0)
            )

        if objs:
            results.append(f"b -o,md {event} {anim} {' '.join(objs)}")
        else:
            results.append(f"b -md {event} {anim}")

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w+', encoding='utf-8') as f:
        f.write('\n'.join(results))
    return {
        "cmd_lines": results,
        "anim_stages": anim_stages
    }
