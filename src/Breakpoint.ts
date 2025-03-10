import { RangeSet, StateEffect, StateField } from "@codemirror/state";

import { EditorView, gutter, GutterMarker } from "@codemirror/view";

const breakpointMarker = new (class extends GutterMarker {
  toDOM() {
    const container = document.createElement("div");
    container.style.display = "flex";
    container.style.alignItems = "center";
    container.style.justifyContent = "center";
    container.style.height = "100%";

    const circle = document.createElement("span");
    circle.style.display = "inline-block";
    circle.style.width = "0.75em";
    circle.style.height = "0.75em";
    circle.style.borderRadius = "50%";
    circle.classList = "cm-breakpoint-marker";

    container.appendChild(circle);
    return container;
  }
})();

const breakpointEffect = StateEffect.define<{ pos: number; on: boolean }>({
  map: (val, mapping) => ({ pos: mapping.mapPos(val.pos), on: val.on }),
});

export const breakpointState = StateField.define<RangeSet<GutterMarker>>({
  create() {
    return RangeSet.empty;
  },
  update(set, transaction) {
    set = set.map(transaction.changes);
    for (let e of transaction.effects) {
      if (e.is(breakpointEffect)) {
        if (e.value.on)
          set = set.update({
            add: [breakpointMarker.range(e.value.pos)],
          });
        else set = set.update({ filter: (from) => from != e.value.pos });
      }
    }
    return set;
  },
});

export const breakpointGutter = [
  breakpointState,
  gutter({
    class: "cm-breakpoint-gutter",
    markers: (v) => v.state.field(breakpointState),
    initialSpacer: () => breakpointMarker,
    domEventHandlers: {
      mousedown(view, line) {
        const pos = line.from; // why is it 0-idx'd here
        let breakpoints = view.state.field(breakpointState);
        let hasBreakpoint = false;
        breakpoints.between(pos, pos, () => {
          hasBreakpoint = true;
        });
        view.dispatch({
          effects: breakpointEffect.of({ pos, on: !hasBreakpoint }),
        });
        return true;
      },
    },
  }),
  EditorView.baseTheme({
    ".cm-breakpoint-gutter .cm-gutterElement": {
      color: "red",
      cursor: "default",
      paddingLeft: "0.3em",
    },
  }),
];
