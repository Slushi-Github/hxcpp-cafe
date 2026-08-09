package;

using StringTools;

class Progress {
	private var current:Int;
	private var total:Int;

	public function new(inCurrent:Int, inTotal:Int) {
		current = inCurrent;
		total = inTotal;
	}

	public function progress(inCurrent:Int) {
		current += inCurrent;
	}

	public function getProgress() {
		final percent = current / total;
		final pct = Std.int(percent * 1000) / 10;
		var str = Std.string(pct);
		if (Std.int(pct) == pct) {
			str += ".0";
		}
		return "[" + str + "%]";
	}
}