import ast
import json
import os
from typing import Any, Dict, List
from enum import Enum, Flag


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
        keys = [convert_node(k) for k in node.keys]
        values = [convert_node(v) for v in node.values]
        return dict(zip(keys, values))
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

    config = {
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

            actor_out = {'gender': gender}
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
    Werewolf = 1 << 47
    Wisp = 1 << 48
    Wispmother = 1 << 49
    Wolf = 1 << 50


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


def tags_process(raw_tags: List[str]):
    PositionHelper = ["2futa", "3 girls", "cccf", "cccsf", "ccf", "ccsf", "cf", "cff", "csf", "csfsf", "f", "fcccc", "fcc", "ffffm", "fffm", "ffm", "ffmm", "fm", "fmc",
                      "fmmm", "m", "mfc", "mff", "mmf", "mmmf", "mmmmsf", "mmmsf", "mmsf", "msf", "msfc", "sfsfc", "sfsfm", "sfsfmm", "sfsfsfm", "sfsfsfsfm", "ff", "fff", "fffc", "ffc", "ccm"]

    Races = ["atronach", "bear", "boar", "cat", "chaurus", "chicken", "cow", "crab", "deer", "dog", "dragon", "dragonpriest", "draugr", "falmer", "flameatronach", "fox", "frostatronach", "gargoyle", "giant", "giantspider", "goat", "hag", "hagraven", "horse", "horker", "husky", "icewraith", "mammoth", "netch",
             "rabbit", "reaper", "riekling", "sabrecat", "seeker", "skeever", "slaughterfish", "spider", "spriggan", "stormatronach", "troll", "unicorn", "vampire", "vampirelord", "werewolf", "wispmother", "wolf", "femwerewolf", "chaurushunter", "chaurusreapers", "largespider", "benthiclurker", "ashhopper"]

    helper = []
    race = []
    for tag in raw_tags:
        tag_lower = tag.lower()
        if tag_lower in PositionHelper:
            helper.append(tag_lower)
        elif tag_lower in Races:
            race.append(tag_lower)
    return {"position_helper": helper, "race": race}


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
    processed = {}

    return processed


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
                            action = action.strip()
                            target = target.strip()
                            tag = tag.strip().lower()
                            slate_tags.setdefault(action, {}).setdefault(
                                target, []).append(tag)

    with open(file_path, 'r', encoding='utf-8-sig') as f:
        file_content = f.read()
        try:
            parsed_data = parse_animation_file(file_content)
            processed_data = prase_raw_data(parsed_data)
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")
            raise e
    return processed_data
