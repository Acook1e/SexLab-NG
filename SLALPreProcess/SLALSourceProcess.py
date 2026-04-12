import ast
import json
import os
from typing import Any, Dict, List, Optional, Set
from enum import Enum, Flag

from .logger import Logger
logger = Logger()

ROLE_TYPES = {'Female', 'Male', 'CreatureMale', 'CreatureFemale'}

# ------------------------------------------------------------
# AST 节点转换器：将 ast 节点转为 Python 基本类型，并保留结构信息
# ------------------------------------------------------------


def convert_node(node: ast.AST) -> Any:
    """
    将 AST 节点转换为可 JSON 序列化的 Python 对象。
    对角色类型调用和 Stage 调用返回带有 __type__ 标记的字典。
    """
    if isinstance(node, ast.Constant):
        return node.value
    elif isinstance(node, ast.Name):
        return node.id
    elif isinstance(node, ast.List):
        return [convert_node(elt) for elt in node.elts]
    elif isinstance(node, ast.Tuple):
        return [convert_node(elt) for elt in node.elts]
    elif isinstance(node, ast.Dict):
        result = {}
        for key_node, value_node in zip(node.keys, node.values):
            if key_node is None:
                continue
            result[convert_node(key_node)] = convert_node(value_node)
        return result
    elif isinstance(node, ast.Call):
        func_name = convert_node(node.func)
        args = [convert_node(arg) for arg in node.args]
        keywords = {kw.arg: convert_node(kw.value)
                    for kw in node.keywords if kw.arg}
        # 显式支持所有角色类型
        if func_name in ('Female', 'Male', 'CreatureMale', 'CreatureFemale'):
            return {'__type__': func_name, **keywords}
        elif func_name == 'Stage':
            return {'__type__': 'Stage', 'args': args, **keywords}
        else:
            # 其他函数调用（理论上不会出现，但保留完整性）
            print(f"Warning: Unhandled function call '{func_name}'")
            return {'__type__': func_name, 'args': args, **keywords}
    elif isinstance(node, ast.UnaryOp):
        if isinstance(node.op, ast.USub):
            return -convert_node(node.operand)
        elif isinstance(node.op, ast.UAdd):
            return +convert_node(node.operand)
        else:
            print(
                f"Warning: Unhandled unary operator '{type(node.op).__name__}'")
            return f"<UnaryOp {type(node.op).__name__}>"
    elif isinstance(node, ast.BinOp):
        left = convert_node(node.left)
        right = convert_node(node.right)
        op = type(node.op).__name__
        return f"<BinOp {left} {op} {right}>"
    elif isinstance(node, ast.Attribute):
        return f"{convert_node(node.value)}.{node.attr}"
    else:
        print(f"Warning: Unhandled AST node type '{type(node).__name__}'")
        return f"<{type(node).__name__}>"


def normalize_actor_value(value: Any) -> Any:
    if isinstance(value, str) and value in ROLE_TYPES:
        return {'__type__': value}
    return value


def parse_animation_file(content: str) -> dict:
    '''
    解析源数据文件，提取配置和动画信息，构建结构化的字典输出
    '''
    tree = ast.parse(content)

    config: Dict[str, Any] = {
        'anim_dir': None,
        'id_prefix': None,
        'name_prefix': None,
        'pack_tags': None
    }
    animations = []

    # 第一遍：收集配置和 Animation 调用
    for stmt in tree.body:
        if isinstance(stmt, ast.Expr) and isinstance(stmt.value, ast.Call):
            call = stmt.value
            func_name = convert_node(call.func)
            if func_name == 'anim_dir' and call.args:
                config['anim_dir'] = convert_node(call.args[0])
            elif func_name == 'anim_id_prefix' and call.args:
                config['id_prefix'] = convert_node(call.args[0])
            elif func_name == 'anim_name_prefix' and call.args:
                config['name_prefix'] = convert_node(call.args[0])
            elif func_name == 'common_tags' and call.args:
                tags_str = convert_node(call.args[0])
                # 分割并转为小写
                raw_tags = [t.strip()
                            for t in tags_str.split(',') if t.strip()]
                config['pack_tags'] = [t.lower() for t in raw_tags]
            elif func_name == 'Animation':
                anim_data = {}
                for kw in call.keywords:
                    if not kw.arg:
                        continue
                    value = convert_node(kw.value)
                    if kw.arg.startswith('actor'):
                        value = normalize_actor_value(value)
                    anim_data[kw.arg] = value
                animations.append(anim_data)

    animations_dict = {}
    for anim in animations:
        anim_id = anim.get('id')
        if not anim_id:
            continue

        anim_out = {
            'name': anim.get('name', ''),
            'tags': [],
            'sound': anim.get('sound', '')
        }
        # 处理 tags：字符串 -> 小写列表
        tags_str = anim.get('tags', '')
        if isinstance(tags_str, str):
            raw_tags = [t.strip() for t in tags_str.split(',') if t.strip()]
            anim_out['tags'] = [t.lower() for t in raw_tags]

        # 收集全局 stage_params，转换为字典 {阶段号: 参数}
        global_stages = {}
        if 'stage_params' in anim:
            global_list = anim['stage_params']
            if isinstance(global_list, list):
                for stage_call in global_list:
                    if isinstance(stage_call, dict) and stage_call.get('__type__') == 'Stage':
                        args = stage_call.get('args', [])
                        if args:
                            stage_num = str(args[0])
                            params = {k: v for k, v in stage_call.items(
                            ) if k not in ('__type__', 'args')}
                            global_stages[stage_num] = params

        # 处理所有 actor（actor1, actor2, ...）
        # 找出所有以 'actor' 开头的字段，且值为角色类型（带有 __type__ 标记的字典）
        actor_keys = [
            k for k in anim.keys()
            if k.startswith('actor')
            and isinstance(anim[k], dict)
            and anim[k].get('__type__') is not None
        ]
        for actor_key in actor_keys:
            actor_data = anim[actor_key]
            actor_type = actor_data.get('__type__', '')

            # 根据类型名判断性别：如果包含 'male'（不区分大小写）则为 male，否则 female
            if 'Male' == actor_type:
                gender = 'male'
            elif 'CreatureMale' == actor_type:
                gender = 'creature_male'
            elif 'Female' == actor_type:
                gender = 'female'
            elif 'CreatureFemale' == actor_type:
                gender = 'creature_female'
            else:
                gender = 'female'

            actor_out: Dict[str, Any] = {'gender': gender}
            # 复制角色调用的其他属性（如 strap_on, add_cum 等）
            for attr, value in actor_data.items():
                if attr != '__type__':
                    actor_out[attr] = value.lower() if isinstance(
                        value, str) else value

            # 提取 actor 编号
            try:
                actor_num = int(actor_key.replace('actor', ''))
            except ValueError:
                continue

            # 构建该 actor 自己的 stage_params
            stage_params_key = f'a{actor_num}_stage_params'
            actor_stages = {}
            if stage_params_key in anim:
                stage_list = anim[stage_params_key]
                if isinstance(stage_list, list):
                    for stage_call in stage_list:
                        if isinstance(stage_call, dict) and stage_call.get('__type__') == 'Stage':
                            args = stage_call.get('args', [])
                            if args:
                                stage_num = str(args[0])
                                params = {k: v for k, v in stage_call.items(
                                ) if k not in ('__type__', 'args')}
                                actor_stages[stage_num] = params

            # 合并全局 stage_params：对每个全局阶段号，如果 actor 已有则更新，否则创建新阶段
            for stage_num, global_params in global_stages.items():
                if stage_num in actor_stages:
                    # 合并（全局参数可覆盖 actor 的同名参数）
                    actor_stages[stage_num].update(global_params)
                else:
                    # 创建新阶段，仅包含全局参数
                    actor_stages[stage_num] = global_params.copy()

            # 如果有阶段参数，添加到 actor_out
            if actor_stages:
                actor_out['stage_params'] = actor_stages

            anim_out[actor_key] = actor_out

        animations_dict[anim_id] = anim_out

    result = {
        'anim_dir': config.get('anim_dir'),
        'id_prefix': config.get('id_prefix'),
        'name_prefix': config.get('name_prefix'),
        'pack_tags': config.get('pack_tags', []),
        'animations': animations_dict
    }
    return result


unknown = {}


INTERACT_TAG_BITS = {
    "Kiss": 1 << 0,
    "BreastSucking": 1 << 1,
    "ToeSucking": 1 << 2,
    "Cunnilingus": 1 << 3,
    "Anilingus": 1 << 4,
    "Fellatio": 1 << 5,
    "DeepThroat": 1 << 6,
    "MouthAnimObj": 1 << 7,
    "GropeBreast": 1 << 8,
    "Titfuck": 1 << 9,
    "GropeThigh": 1 << 10,
    "GropeButt": 1 << 11,
    "GropeFoot": 1 << 12,
    "FingerVagina": 1 << 13,
    "FingerAnus": 1 << 14,
    "Handjob": 1 << 15,
    "Masturbation": 1 << 16,
    "HandAnimObj": 1 << 17,
    "Naveljob": 1 << 18,
    "Thighjob": 1 << 19,
    "Frottage": 1 << 20,
    "Footjob": 1 << 21,
    "Tribbing": 1 << 22,
    "Vaginal": 1 << 23,
    "VaginaAnimObj": 1 << 24,
    "Anal": 1 << 25,
    "AnalAnimObj": 1 << 26,
    "PenisAnimObj": 1 << 27,
    "SixtyNine": 1 << 28,
    "Spitroast": 1 << 29,
    "DoublePenetration": 1 << 30,
    "TriplePenetration": 1 << 31,
}


SCENE_TAG_BITS = {
    "LeadIn": 1 << 0,
    "Aggressive": 1 << 1,
    "Forced": 1 << 2,
    "Dominant": 1 << 3,
    "Humiliation": 1 << 4,
    "Loving": 1 << 5,
    "Teasing": 1 << 6,
    "Cowgirl": 1 << 7,
    "ReverseCowgirl": 1 << 8,
    "Missionary": 1 << 9,
    "Doggy": 1 << 10,
    "ProneBone": 1 << 11,
    "LotusPosition": 1 << 12,
    "Spooning": 1 << 13,
    "FaceSitting": 1 << 14,
    "Standing": 1 << 15,
    "Kneeling": 1 << 16,
    "Lying": 1 << 17,
    "Sitting": 1 << 18,
    "Behind": 1 << 19,
    "Facing": 1 << 20,
    "Holding": 1 << 21,
    "Hugging": 1 << 22,
    "Fisting": 1 << 23,
    "Spanking": 1 << 24,
    "Toys": 1 << 25,
    "Magic": 1 << 26,
    "Facial": 1 << 27,
    "Ryona": 1 << 28,
    "Gore": 1 << 29,
    "Oviposition": 1 << 30,
    "Oral": 1 << 31,
    "Vaginal": 1 << 32,
    "Anal": 1 << 33,
    "Feet": 1 << 34,
}


POSITION_TAGS = {
    "2futa", "3 girls", "cccf", "ccccf", "cccsf", "ccf", "ccsf", "cf", "cff", "csf", "csfsf", "f",
    "fcccc", "fcc", "ffffm", "fffm", "ffm", "ffmm", "fm", "fmc", "fmmm", "m", "mfc", "mff",
    "mmf", "mmmf", "mmmmsf", "mmmsf", "mmsf", "msf", "msfc", "sfsfc", "sfsfm", "sfsfmm",
    "sfsfsfm", "sfsfsfsfm", "ff", "fff", "fffc", "ffc", "ccm", "mf"
}

RACE_TAGS = {
    "atronach", "bear", "boar", "cat", "chaurus", "chicken", "cow", "crab", "deer", "dog", "dragon",
    "dragonpriest", "draugr", "falmer", "flameatronach", "fox", "frostatronach", "gargoyle", "giant",
    "giantspider", "goat", "hag", "hagraven", "horse", "horker", "husky", "icewraith", "lurker",
    "mammoth", "netch", "rabbit", "reaper", "riekling", "sabrecat", "seeker", "skeever",
    "slaughterfish", "spider", "spriggan", "stormatronach", "troll", "unicorn", "vampire",
    "vampirelord", "werewolf", "wispmother", "wolf", "femwerewolf", "chaurushunter", "chaurusreapers",
    "largespider", "benthiclurker", "ashhopper", "futa", "male", "female", "canine"
}

SCALE_TAGS = {"petite", "bigguy", "loli", "shota"}
SCENE_TYPE_TAGS = {"aggressive", "rape", "leadin", "aggressivedefault"}
SKIP_TAGS = {"test"}

INTERACT_TAG_ALIASES = {
    "kiss": {"Kiss"},
    "kissing": {"Kiss"},
    "blowjob": {"Fellatio"},
    "fellatio": {"Fellatio"},
    "blowbang": {"Fellatio"},
    "facefuck": {"Fellatio"},
    "deepthroat": {"DeepThroat"},
    "cunnilingus": {"Cunnilingus"},
    "anilingus": {"Anilingus"},
    "analinggus": {"Anilingus"},
    "rimjob": {"Anilingus"},
    "handjob": {"Handjob"},
    "balljob": {"Handjob"},
    "masturbation": {"Masturbation"},
    "boobsuck": {"BreastSucking"},
    "boobjob": {"Titfuck"},
    "titfuck": {"Titfuck"},
    "thighjob": {"Thighjob"},
    "frottage": {"Frottage"},
    "frotting": {"Frottage"},
    "bodyjob": {"Frottage"},
    "footjob": {"Footjob"},
    "toesucking": {"ToeSucking"},
    "tribbing": {"Tribbing"},
    "scissoring": {"Tribbing"},
    "vaginal": {"Vaginal"},
    "doublevag": {"Vaginal"},
    "pussy": {"Vaginal"},
    "anal": {"Anal"},
    "anus": {"Anal"},
    "analcreampie": {"Anal"},
    "69": {"SixtyNine"},
    "spitroast": {"Spitroast"},
    "doublepen": {"DoublePenetration"},
    "dp": {"DoublePenetration"},
    "triplepen": {"TriplePenetration"},
    "tripplepen": {"TriplePenetration"},
    "naveljob": {"Naveljob"},
}

SCENE_TAG_ALIASES = {
    "leadin": {"LeadIn"},
    "aggressive": {"Aggressive"},
    "aggressivedefault": {"Aggressive"},
    "rape": {"Aggressive", "Forced"},
    "rough": {"Aggressive"},
    "forced": {"Forced"},
    "bound": {"Forced"},
    "cuffs": {"Forced"},
    "cuffed": {"Forced"},
    "binding": {"Forced"},
    "armbinder": {"Forced"},
    "yoke": {"Forced"},
    "dd": {"Forced"},
    "deviousdevice": {"Forced"},
    "defeated": {"Forced"},
    "pillory": {"Forced"},
    "pillorylow": {"Forced"},
    "wheel": {"Forced"},
    "tiltedwheel": {"Forced"},
    "femdom": {"Dominant"},
    "domsub": {"Dominant"},
    "service": {"Dominant"},
    "conquering": {"Dominant"},
    "loving": {"Loving"},
    "love": {"Loving"},
    "cute": {"Loving"},
    "teasing": {"Teasing"},
    "cowgirl": {"Cowgirl"},
    "reversecowgirl": {"ReverseCowgirl"},
    "matingpress": {"Missionary", "Lying", "Facing"},
    "missionary": {"Missionary", "Facing"},
    "doggy": {"Doggy", "Behind"},
    "doggystyle": {"Doggy", "Behind"},
    "pronebone": {"ProneBone"},
    "lotus": {"LotusPosition"},
    "spooning": {"Spooning", "Lying"},
    "facesit": {"FaceSitting", "Oral"},
    "facesitting": {"FaceSitting", "Oral"},
    "standing": {"Standing"},
    "kneeling": {"Kneeling"},
    "laying": {"Lying"},
    "lying": {"Lying"},
    "onback": {"Lying"},
    "sitting": {"Sitting"},
    "behind": {"Behind"},
    "facing": {"Facing"},
    "holding": {"Holding"},
    "hugging": {"Hugging"},
    "fisting": {"Fisting"},
    "spanking": {"Spanking"},
    "object": {"Toys"},
    "animobject": {"Toys"},
    "dildo": {"Toys"},
    "sextoy": {"Toys"},
    "staff": {"Toys"},
    "magic": {"Magic"},
    "mage": {"Magic"},
    "facial": {"Facial"},
    "oviposition": {"Oviposition"},
    "eggs": {"Oviposition"},
    "ryona": {"Ryona"},
    "gore": {"Gore"},
    "oral": {"Oral"},
    "licking": {"Oral"},
    "sucking": {"Oral"},
    "blowjob": {"Oral"},
    "blowbang": {"Oral"},
    "facefuck": {"Oral"},
    "cunnilingus": {"Oral"},
    "anilingus": {"Oral"},
    "analinggus": {"Oral"},
    "vaginal": {"Vaginal"},
    "doublevag": {"Vaginal"},
    "anal": {"Anal"},
    "anus": {"Anal"},
    "footjob": {"Feet"},
    "feetcum": {"Feet"},
}

KNOWN_META_TAGS = {
    "sex", "dirty", "straight", "bestiality", "creature", "solo", "threesome", "group", "gangbang",
    "orgy", "lesbian", "movingdick", "abc", "creampie", "analcreampie", "cuminmouth", "chestcum",
    "cumonbody", "aircum", "cuminside", "mouth", "penis", "object", "animobject", "furniture", "knotted",
    "pose", "powerbomb", "piledriver", "hanging", "gallows", "rider", "sideways", "floating", "threeway",
    "bukkake", "doublebed", "throne", "nelson", "obedient", "hands", "sexdoll", "frozen", "choke",
    "panicforever", "earlyclimax", "multiclimax", "dayelyte", "xandero", "llabsky", "movingdick", "groping",
    "foreplay", "fingering", "leito",
}

OBJECT_TAG_HINTS = {"object", "animobject", "dildo", "sextoy", "staff"}
ORAL_TAG_HINTS = {"oral", "blowjob", "facefuck", "blowbang",
                  "cunnilingus", "anilingus", "analinggus", "licking", "sucking"}
VAGINAL_TAG_HINTS = {"vaginal", "doublevag", "pussy"}
ANAL_TAG_HINTS = {"anal", "anus", "analcreampie"}
FEET_TAG_HINTS = {"footjob", "feetcum", "toesucking"}


def normalize_tag(tag: str) -> str:
    return ''.join(ch for ch in tag.strip().lower() if ch.isalnum())


def build_mask(bit_map: Dict[str, int], names: Set[str]) -> int:
    mask = 0
    for name in names:
        mask |= bit_map.get(name, 0)
    return mask


def collect_alias_mappings(normalized_tags: Set[str], alias_map: Dict[str, Set[str]]) -> Set[str]:
    mapped: Set[str] = set()
    for tag in normalized_tags:
        mapped.update(alias_map.get(tag, set()))
    return mapped


def actor_supports_vagina(position: dict) -> bool:
    return position.get("gender", GenderType.Unknown.value) in {
        GenderType.Female.value,
        GenderType.Futa.value,
        GenderType.CreatureFemale.value,
    }


def actor_supports_penis(position: dict) -> bool:
    return position.get("gender", GenderType.Unknown.value) in {
        GenderType.Male.value,
        GenderType.Futa.value,
        GenderType.CreatureMale.value,
    }


def filter_position_interact_names(names: Set[str], position: dict) -> Set[str]:
    filtered = set(names)
    if "VaginaAnimObj" in filtered and not actor_supports_vagina(position):
        filtered.remove("VaginaAnimObj")
    if "PenisAnimObj" in filtered and not actor_supports_penis(position):
        filtered.remove("PenisAnimObj")
    return filtered


def resolve_interact_tag_names(raw_tags: List[str]) -> Set[str]:
    normalized_tags = {normalize_tag(tag) for tag in raw_tags if tag}
    names = collect_alias_mappings(normalized_tags, INTERACT_TAG_ALIASES)

    if "fingering" in normalized_tags:
        if normalized_tags & ANAL_TAG_HINTS:
            names.add("FingerAnus")
        if normalized_tags & VAGINAL_TAG_HINTS or not (normalized_tags & ANAL_TAG_HINTS):
            names.add("FingerVagina")

    if normalized_tags & OBJECT_TAG_HINTS:
        if normalized_tags & ORAL_TAG_HINTS:
            names.add("MouthAnimObj")
        if "handjob" in normalized_tags or "balljob" in normalized_tags or "masturbation" in normalized_tags:
            names.add("HandAnimObj")
        if normalized_tags & VAGINAL_TAG_HINTS:
            names.add("VaginaAnimObj")
        if normalized_tags & ANAL_TAG_HINTS:
            names.add("AnalAnimObj")
        if "penis" in normalized_tags:
            names.add("PenisAnimObj")

    return names


def resolve_scene_tag_names(raw_tags: List[str], interact_names: Set[str]) -> Set[str]:
    normalized_tags = {normalize_tag(tag) for tag in raw_tags if tag}
    names = collect_alias_mappings(normalized_tags, SCENE_TAG_ALIASES)

    if "reverse" in normalized_tags and "cowgirl" in normalized_tags:
        names.add("ReverseCowgirl")
    if "matingpress" in normalized_tags:
        names.update({"Missionary", "Lying", "Facing"})
    if "sideways" in normalized_tags:
        names.update({"Spooning", "Lying"})
    if "hanging" in normalized_tags:
        names.add("Standing")

    if interact_names & {"Fellatio", "DeepThroat", "Cunnilingus", "Anilingus", "BreastSucking", "ToeSucking", "SixtyNine"}:
        names.add("Oral")
    if interact_names & {"Vaginal", "FingerVagina", "Tribbing", "VaginaAnimObj", "DoublePenetration", "TriplePenetration"}:
        names.add("Vaginal")
    if interact_names & {"Anal", "FingerAnus", "AnalAnimObj", "DoublePenetration", "TriplePenetration"}:
        names.add("Anal")
    if interact_names & {"Footjob", "ToeSucking", "GropeFoot"}:
        names.add("Feet")

    return names


def resolve_known_tags(raw_tags: List[str], extra_known_tags: Optional[Set[str]] = None) -> Set[str]:
    known = {normalize_tag(tag) for tag in POSITION_TAGS |
             RACE_TAGS | SCALE_TAGS | SCENE_TYPE_TAGS | SKIP_TAGS}
    known.update(INTERACT_TAG_ALIASES.keys())
    known.update(SCENE_TAG_ALIASES.keys())
    known.update(KNOWN_META_TAGS)
    if extra_known_tags:
        known.update(extra_known_tags)
    return known


def resolve_undefined_tags(raw_tags: List[str], extra_known_tags: Optional[Set[str]] = None) -> List[str]:
    known = resolve_known_tags(raw_tags, extra_known_tags)
    undefined = []
    for tag in raw_tags:
        lowered = tag.strip().lower()
        normalized = normalize_tag(lowered)
        if not normalized or normalized in known:
            continue
        undefined.append(lowered)
    return sorted(set(undefined))


def tags_process(raw_tags: List[str], extra_known_tags: Optional[Set[str]] = None):
    position = []
    race = []
    scene = []
    scale = []

    for tag in raw_tags:
        tag_lower = tag.lower().strip()
        if tag_lower in POSITION_TAGS:
            position.append(tag_lower)
        elif tag_lower in RACE_TAGS:
            race.append(tag_lower)
        elif tag_lower in SCENE_TYPE_TAGS:
            scene.append(tag_lower)
        elif tag_lower in SCALE_TAGS:
            scale.append(tag_lower)

    return {
        "position": position,
        "race": race,
        "scene": scene,
        "scale": scale,
        "undefined": resolve_undefined_tags(raw_tags, extra_known_tags),
    }


def resolve_slate_targets(scene_name: str, event_prefix: str, positions: Dict[str, dict]):
    action_keys = []
    for value in [event_prefix, scene_name]:
        key = normalize_tag(value)
        if key and key not in action_keys:
            action_keys.append(key)

    scene_tags: List[str] = []
    position_tags: Dict[str, List[str]] = {
        actor_key: [] for actor_key in positions.keys()}
    unmatched_tags: List[str] = []

    if not slate_tags:
        return scene_tags, position_tags, unmatched_tags

    actor_aliases: Dict[str, Set[str]] = {}
    for actor_key, position in positions.items():
        actor_index = actor_key.replace("actor", "")
        aliases = {
            normalize_tag(actor_key),
            normalize_tag(f"a{actor_index}"),
            normalize_tag(actor_index),
        }
        gender = position.get("gender", GenderType.Unknown.value)
        if gender == GenderType.Female.value:
            aliases.add("female")
        elif gender == GenderType.Male.value:
            aliases.add("male")
        elif gender == GenderType.Futa.value:
            aliases.update({"futa", "female", "male"})
        elif gender == GenderType.CreatureMale.value:
            aliases.update({"creaturemale", "male"})
        elif gender == GenderType.CreatureFemale.value:
            aliases.update({"creaturefemale", "female"})
        actor_aliases[actor_key] = aliases

    for action_key in action_keys:
        target_map = slate_tags.get(action_key, {})
        for target_key, tags in target_map.items():
            if target_key in {"", "all", "global", "scene", "actors", "*"}:
                scene_tags.extend(tags)
                continue

            matched = False
            for actor_key, aliases in actor_aliases.items():
                if target_key in aliases:
                    position_tags[actor_key].extend(tags)
                    matched = True
            if not matched:
                unmatched_tags.extend(tags)

    return scene_tags, position_tags, unmatched_tags


class Race(Flag):
    Unknown = 0
    Human = 1 << 0
    AshHopper = 1 << 1
    Bear = 1 << 2
    Boar = 1 << 3
    BoarMounted = 1 << 4
    Chaurus = 1 << 5
    ChaurusHunter = 1 << 6
    ChaurusReaper = 1 << 7
    Chicken = 1 << 8
    Cow = 1 << 9
    Deer = 1 << 10
    Dog = 1 << 11
    Dragon = 1 << 12
    DragonPriest = 1 << 13
    Draugr = 1 << 14
    DwarvenBallista = 1 << 15
    DwarvenCenturion = 1 << 16
    DwarvenSphere = 1 << 17
    DwarvenSpider = 1 << 18
    Falmer = 1 << 19
    FlameAtronach = 1 << 20
    Fox = 1 << 21
    FrostAtronach = 1 << 22
    Gargoyle = 1 << 23
    Giant = 1 << 24
    GiantSpider = 1 << 25
    Goat = 1 << 26
    Hagraven = 1 << 27
    Hare = 1 << 28
    Horker = 1 << 29
    Horse = 1 << 30
    IceWraith = 1 << 31
    LargeSpider = 1 << 32
    Lurker = 1 << 33
    Mammoth = 1 << 34
    Mudcrab = 1 << 35
    Netch = 1 << 36
    Riekling = 1 << 37
    Sabrecat = 1 << 38
    Seeker = 1 << 39
    Skeever = 1 << 40
    Slaughterfish = 1 << 41
    Spider = 1 << 42
    Spriggan = 1 << 43
    StormAtronach = 1 << 44
    Troll = 1 << 45
    VampireLord = 1 << 46
    Werebear = 1 << 47
    Werewolf = 1 << 48
    Wisp = 1 << 49
    Wispmother = 1 << 50
    Wolf = 1 << 51


def to_race(race: str) -> int:
    res = Race.Unknown
    match (race.lower()):
        case "ashhoppers":
            res = Race.AshHopper
        case "bears":
            res = Race.Bear
        case "boars":
            res = Race.Boar
        case "boarsany":
            res = Race.Boar | Race.BoarMounted
        case "boarmounted":
            res = Race.BoarMounted
        case "canines":
            res = Race.Dog | Race.Fox | Race.Wolf
        case "chaurus":
            res = Race.Chaurus
        case "chaurushunters":
            res = Race.ChaurusHunter
        case "chaurusreapers":
            res = Race.ChaurusReaper
        case "chickens":
            res = Race.Chicken
        case "cows":
            res = Race.Cow
        case "deers":
            res = Race.Deer
        case "dogs":
            res = Race.Dog
        case "dragons":
            res = Race.Dragon
        case "dragonpriests":
            res = Race.DragonPriest
        case "draugrs":
            res = Race.Draugr
        case "dwarvenballistas":
            res = Race.DwarvenBallista
        case "dwarvencenturions":
            res = Race.DwarvenCenturion
        case "dwarvenspheres":
            res = Race.DwarvenSphere
        case "dwarvenspiders":
            res = Race.DwarvenSpider
        case "falmer":
            res = Race.Falmer
        case "flameatronach":
            res = Race.FlameAtronach
        case "foxes":
            res = Race.Fox
        case "frostatronach":
            res = Race.FrostAtronach
        case "gargoyles":
            res = Race.Gargoyle
        case "giantspiders":
            res = Race.GiantSpider
        case "giants":
            res = Race.Giant
        case "goats":
            res = Race.Goat
        case "hagravens":
            res = Race.Hagraven
        case "rabbits" | "hares":
            res = Race.Hare
        case "horkers":
            res = Race.Horker
        case "horses":
            res = Race.Horse
        case "human":
            res = Race.Human
        case "icewraiths":
            res = Race.IceWraith
        case "largespiders":
            res = Race.LargeSpider
        case "lurkers":
            res = Race.Lurker
        case "mammoths":
            res = Race.Mammoth
        case "netches":
            res = Race.Netch
        case "rieklings":
            res = Race.Riekling
        case "sabrecats":
            res = Race.Sabrecat
        case "seekers":
            res = Race.Seeker
        case "skeevers":
            res = Race.Skeever
        case "slaughterfishes":
            res = Race.Slaughterfish
        case "spiders":
            res = Race.Spider
        case "spriggans":
            res = Race.Spriggan
        case "stormatronach":
            res = Race.StormAtronach
        case "trolls":
            res = Race.Troll
        case "vampirelords":
            res = Race.VampireLord
        case "werebears":
            res = Race.Werebear
        case "werewolves":
            res = Race.Werewolf
        case "wispmothers":
            res = Race.Wispmother
        case "wolves":
            res = Race.Wolf
        case _:
            unknown.setdefault("race", []).append(race)
    return res.value


class GenderType(Enum):
    Unknown = 0
    Female = 1
    Male = 2
    Futa = 3
    CreatureMale = 4
    CreatureFemale = 5


def to_gender(gender: str) -> int:
    res = GenderType.Unknown
    match (gender):
        case "female":
            res = GenderType.Female
        case "male":
            res = GenderType.Male
        case "creature_male":
            res = GenderType.CreatureMale
        case "creature_female":
            res = GenderType.CreatureFemale
    return res.value


def prase_raw_data(raw_data: dict) -> dict:
    '''
    将数据格式化为SexLab NG需要的结构
    '''
    result = {}
    anim_dir = raw_data.get("anim_dir", "")
    id_prefix = raw_data.get("id_prefix", "")
    name_prefix = raw_data.get("name_prefix", "")

    author = name_prefix.strip() if name_prefix else "Unknown"
    pack_tags = raw_data.get("pack_tags", [])
    result["author"] = author
    result["pack_tag"] = pack_tags[0] if pack_tags else ""
    result["anim_dir"] = anim_dir
    result["scenes"] = dict()
    for id, anim in raw_data["animations"].items():
        name = name_prefix + anim["name"]
        event = id_prefix + id
        total_stages = 0
        total_actors = 0
        positions = {}
        races = 0
        for actor_key in range(1, 10):
            key = f"actor{actor_key}"
            if key in anim:
                position = {}
                position["gender"] = to_gender(anim[key]["gender"])
                position["race"] = to_race(anim[key].get("race", "human"))
                races |= position["race"]
                position["be_cumed"] = anim[key].get("add_cum", "none")
                position["scale"] = 1.0
                position["stage_params"] = {}
                for stage_num, param in anim[key].get("stage_params", {}).items():
                    for param_name, param_value in param.items():
                        if param_name == "animvars" or param_name == "object":
                            continue
                        position["stage_params"].setdefault(
                            stage_num, {})[param_name] = param_value
                positions[str(key)] = position
            else:
                total_actors = actor_key - 1
                break

        result["scenes"][name] = {
            "event_prefix": event,
            "tags": anim["tags"],
            "total_actors": total_actors,
            "total_stages": total_stages,
            "races": races,
            "positions": positions
        }
    return result


slate_tags = {}


def preprocess_data(data: dict):
    '''
    对解析后的数据预处理，如标签分类、位置统计等
    '''
    extra_known_tags = {
        normalize_tag(data.get("pack_tag", "")),
        normalize_tag(data.get("anim_dir", "")),
        normalize_tag(data.get("author", "")),
    }
    extra_known_tags.discard("")

    for scene_name, scene_data in data["scenes"].items():
        tags = [tag.lower().strip()
                for tag in scene_data.get("tags", []) if isinstance(tag, str)]
        scene_data["tags"] = tags

        tag_info = tags_process(tags, extra_known_tags)
        if tag_info["scale"]:
            if tag_info["position"]:
                position_tag = tag_info["position"][0]
                loli_count = position_tag.count("sf")
                shota_count = position_tag.count("sm")
                scene_female = 0
                scene_male = 0
                for actor_key, actor_data in scene_data["positions"].items():
                    if actor_data["gender"] == 1:
                        scene_female += 1
                    elif actor_data["gender"] == 2:
                        scene_male += 1
                if loli_count == scene_female:
                    logger.info(f"Scene '{scene_name}' scaled as petite")
                    for actor_key, actor_data in scene_data["positions"].items():
                        if actor_data["gender"] == 1:
                            actor_data["scale"] = 0.8
                elif shota_count == scene_male:
                    logger.info(f"Scene '{scene_name}' scaled as petite")
                    for actor_key, actor_data in scene_data["positions"].items():
                        if actor_data["gender"] == 2:
                            actor_data["scale"] = 0.8
                else:
                    logger.warn(
                        f"Scene '{scene_name}' has scale tag but position tag does not match, skipping scale")

        scene_slate_tags, position_slate_tags, unmatched_slate_tags = resolve_slate_targets(
            scene_name,
            scene_data.get("event_prefix", ""),
            scene_data["positions"],
        )

        shared_interact_names = resolve_interact_tag_names(tags)
        scene_tag_names = resolve_scene_tag_names(tags, shared_interact_names)

        slate_scene_interact_names = resolve_interact_tag_names(
            scene_slate_tags)
        shared_interact_names.update(slate_scene_interact_names)
        scene_tag_names.update(resolve_scene_tag_names(
            scene_slate_tags, slate_scene_interact_names))

        scene_unknown_tags = set(tag_info["undefined"])
        scene_unknown_tags.update(resolve_undefined_tags(
            scene_slate_tags, extra_known_tags))
        scene_unknown_tags.update(resolve_undefined_tags(
            unmatched_slate_tags, extra_known_tags))

        all_interact_names = set(shared_interact_names)
        for actor_key, actor_data in scene_data["positions"].items():
            actor_slate_tags = position_slate_tags.get(actor_key, [])
            actor_interact_names = set(shared_interact_names)
            actor_interact_names.update(
                resolve_interact_tag_names(actor_slate_tags))
            actor_interact_names = filter_position_interact_names(
                actor_interact_names, actor_data)

            actor_undefined_tags = resolve_undefined_tags(
                actor_slate_tags, extra_known_tags)
            actor_data["interact_tags"] = build_mask(
                INTERACT_TAG_BITS, actor_interact_names)
            actor_data["interact_tag_names"] = sorted(actor_interact_names)
            actor_data["undefined_tags"] = actor_undefined_tags
            if actor_slate_tags:
                actor_data["slate_tags"] = sorted(set(actor_slate_tags))

            all_interact_names.update(actor_interact_names)
            scene_unknown_tags.update(actor_undefined_tags)

        scene_tag_names.update(resolve_scene_tag_names(
            tags + scene_slate_tags, all_interact_names))
        scene_data["scene_tags"] = build_mask(SCENE_TAG_BITS, scene_tag_names)
        scene_data["scene_tag_names"] = sorted(scene_tag_names)
        scene_data["undefined_tags"] = sorted(scene_unknown_tags)
        if scene_slate_tags:
            scene_data["slate_tags"] = sorted(set(scene_slate_tags))
    return data


def preprocess_slal_source(file_path: str):
    if not slate_tags:
        slate_dir = os.path.join(
            os.path.dirname(__file__), "slate")
        if os.path.isdir(slate_dir):
            for fname in os.listdir(slate_dir):
                if fname.endswith(".json"):
                    with open(os.path.join(slate_dir, fname), 'r', encoding='utf-8-sig') as f:
                        data = json.load(f)
                        for slate in data["stringList"]["slate.actionlog"]:
                            action, target, tag = slate.split(',', 2)
                            action = normalize_tag(action)
                            target = normalize_tag(target)
                            tag = tag.strip().lower()
                            slate_tags.setdefault(action, {}).setdefault(
                                target, []).append(tag)

    with open(file_path, 'r', encoding='utf-8-sig') as f:
        file_content = f.read()
        try:
            parsed_data = parse_animation_file(file_content)
            processed_data = prase_raw_data(parsed_data)
            processed_data = preprocess_data(processed_data)
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")
            raise e

    return processed_data
