import { StateEffect, StateField } from "@codemirror/state";
import { Decoration, DecorationSet, EditorView } from "@codemirror/view";

export const lineHighlightEffect = StateEffect.define<number | null>();
export const lineHighlightState = StateField.define<DecorationSet>({
  create() {
    return Decoration.none;
  },
  // NOTE: this only works if there is only one highlighted line!
  update(highlights, tr) {
    for (let effect of tr.effects) {
      if (effect.is(lineHighlightEffect)) {
        if (
          effect.value &&
          effect.value >= 1 &&
          effect.value <= tr.state.doc.lines
        ) {
          let line = tr.state.doc.line(effect.value);
          return Decoration.set([
            Decoration.line({ class: "cm-debugging" }).range(
              line.from,
              line.from,
            ),
          ]);
        } else return Decoration.none;
      }
    }
    return highlights.map(tr.changes);
  },
  provide: (f) => EditorView.decorations.from(f),
});
