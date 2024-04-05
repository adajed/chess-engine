import argparse
import re
import time

from db import Database, CHUNK_FIELDS

ENTER_SEARCH_REGEX = re.compile(
    r"^\[(\d+)\] ENTER (QUIESCENCE_SEARCH|SEARCH) depth=(\d+) alpha=(-?\d+) beta=(-?\d+) pvNode=([01]) fen=(.*)$")
EXIT_SEARCH_REGEX = re.compile(
    r"^\[(\d+)\] EXIT (QUIESCENCE_SEARCH|SEARCH) score=(-?\d+)$")
DO_MOVE_REGEX = re.compile(
    r"^\[(\d+)\] DO MOVE (\w+) alpha=(-?\d+) beta=(-?\d+)$")
UNDO_MOVE_REGEX = re.compile(r"^\[(\d+)\] UNDO MOVE (\w+)$")
CACHE_HIT_REGEX = re.compile(
    r"^\[(\d+)\] CACHE HIT score=(-?\d+) depth=(\d+) flag=(\d+) move=(\w+)$")
MOVE_ORDER_REGEX = re.compile(r"^\[(\d+)\] MOVE ORDER (.*)$")
POSITION_REGEX = re.compile(r"^\[(\d+)\] POSITION score=(-?\d+)$")
BEST_MOVE_REGEX = re.compile(r"\[(\d+)\] BEST MOVE (\w+)$")
PV_LIST_REGEX = re.compile(r"\[(\d+)\] PV LIST (.*)$")
RAZORING_REGEX = re.compile(r"\[(\d+)\] RAZORING$")
FUTILITY_REGEX = re.compile(r"\[(\d+)\] FUTILITY PRUNING$")
NODES_SEARCHED_REGEX = re.compile(r"\[(\d+)\] NODES SEARCHED (\d+)$")
IMPROVING_REGEX = re.compile(r"\[(\d+)\] IMPROVING ([01])$")


def parseArguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--logfile", "-l", type=str, required=True)
    parser.add_argument("--output", "-o", type=str, default="logs.sqlite")
    parser.add_argument("--max-ply", "-d", type=int, default=None)
    return parser.parse_args()


class LineGenerator:
    def __init__(self, filepath):
        self.__fd = open(filepath, "r")
        self.__line = self.__fd.readline()

    def hasNext(self):
        return self.__line != ''

    def next(self):
        line = self.__line
        self.__line = self.__fd.readline()
        return line


class Parser:
    def __init__(self, filepath, db, maxPly):
        self._lineGenerator = LineGenerator(filepath)
        self._db = db
        self._maxPly = 40 if maxPly is None else maxPly
        self._chunkStack = [{"depth": -1, "ply": 0}]
        self._moveStack = []

    def parse(self):
        nodeIds = []

        while self._lineGenerator.hasNext():
            line = self._lineGenerator.next()

            if (self._tryParseEnterSearch(line)):
                if (self._get("ply") > self._maxPly):
                    continue
                childIds = self.parse()
                id_ = self._db.insert_chunk(self._chunkStack.pop())
                self._db.insert_connections(id_, childIds)
                nodeIds.append(id_)
            elif (self._tryParseExitSearch(line)):
                return nodeIds
            elif (self._tryParseDoMove(line)):
                pass
            elif (self._tryParseUndoMove(line)):
                pass
            elif (self._tryParseMoveOrder(line)):
                pass
            elif (self._tryParsePvList(line)):
                pass
            elif (self._tryParseCache(line)):
                pass
            elif (self._tryParsePosition(line)):
                pass
            elif (self._tryParseBestMove(line)):
                pass
            elif (self._tryParseRazoring(line)):
                pass
            elif (self._tryParseFutilityPruning(line)):
                pass
            elif (self._tryParseNodesSearched(line)):
                pass
            elif (self._tryParseImproving(line)):
                pass

        return nodeIds

    def _get(self, key):
        return self._chunkStack[-1][key]

    def _updateChunk(self, key, value):
        self._chunkStack[-1][key] = value

    def _tryParseEnterSearch(self, line):
        if (m := ENTER_SEARCH_REGEX.match(line)):
            self._chunkStack.append(dict())
            for field in CHUNK_FIELDS:
                self._updateChunk(field, None)
            self._updateChunk("ply", int(m.group(1)))
            self._updateChunk("depth", int(m.group(3)))
            self._updateChunk("alpha", int(m.group(4)))
            self._updateChunk("beta", int(m.group(5)))
            self._updateChunk("isPV", int(m.group(6)))
            self._updateChunk("fen", m.group(7))
            self._updateChunk("razoring", 0)
            self._updateChunk("futility", 0)
            self._updateChunk("nodes_searched", 0)
            self._updateChunk("improving", 0)
            return True
        return False

    def _tryParseExitSearch(self, line):
        if (m := EXIT_SEARCH_REGEX.match(line)):
            if int(m.group(1)) == self._get("ply"):
                self._updateChunk("score", int(m.group(3)))
                if len(self._moveStack) > 0:
                    self._updateChunk("move", self._moveStack[-1])
                return True
        return False

    def _tryParseDoMove(self, line):
        if (m := DO_MOVE_REGEX.match(line)):
            if (int(m.group(1)) == self._get("ply")):
                self._moveStack.append(m.group(2))
                return True
        return False

    def _tryParseUndoMove(self, line):
        if (m := UNDO_MOVE_REGEX.match(line)):
            if (int(m.group(1)) == self._get("ply")):
                assert(self._moveStack[-1] == m.group(2))
                self._moveStack.pop()
                return True
        return False

    def _tryParseCache(self, line):
        if (m := CACHE_HIT_REGEX.match(line)):
            if (int(m.group(1)) == self._get("ply")):
                self._updateChunk("cache_score", int(m.group(2)))
                self._updateChunk("cache_depth", int(m.group(3)))
                self._updateChunk("cache_flag", int(m.group(4)))
                self._updateChunk("cache_move", m.group(5))
                return True
        return False

    def _tryParseField(self, regex, field, f = lambda m: m.group(2)):
        def handle(line):
            if (m := regex.match(line)):
                if (int(m.group(1)) == self._get("ply")):
                    self._updateChunk(field, f(m))
                    return True
            return False
        return handle

    def _tryParsePosition(self, line):
        return self._tryParseField(POSITION_REGEX, "static_eval", lambda m: int(m.group(2)))(line)

    def _tryParseMoveOrder(self, line):
        return self._tryParseField(MOVE_ORDER_REGEX, "move_order")(line)

    def _tryParsePvList(self, line):
        return self._tryParseField(PV_LIST_REGEX, "pv_list")(line)

    def _tryParseBestMove(self, line):
        return self._tryParseField(BEST_MOVE_REGEX, "best_move")(line)

    def _tryParseRazoring(self, line):
        return self._tryParseField(RAZORING_REGEX, "razoring", lambda _: 1)(line)

    def _tryParseFutilityPruning(self, line):
        return self._tryParseField(FUTILITY_REGEX, "futility", lambda _: 1)(line)

    def _tryParseNodesSearched(self, line):
        return self._tryParseField(NODES_SEARCHED_REGEX, "nodes_searched", lambda m: int(m.group(2)))(line)

    def _tryParseImproving(self, line):
        return self._tryParseField(IMPROVING_REGEX, "improving", lambda m: int(m.group(2)))(line)



def main():
    args = parseArguments()

    db = Database(args.output)
    db.create_tables()

    parser = Parser(args.logfile, db, args.max_ply)
    nodeIds = parser.parse()
    chunkData = {field: None for field in CHUNK_FIELDS}
    chunkData["depth"] = -1
    id_ = db.insert_chunk(chunkData)
    db.insert_connections(id_, nodeIds)
    db.get_conn().commit()


if __name__ == "__main__":
    start = time.time()
    main()
    end = time.time()
    print(f"Duration: {end - start}s")
