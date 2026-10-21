import QtQuick

import App.Theme 0.1

// One shape per status kind so state never relies on colour alone:
// ok = filled dot, safe = check, caution = triangle, critical = octagon,
// busy = turning ring, off = hollow ring, unknown = dashed ring.
Canvas {
    id: icon

    property string kind: "unknown"
    property color colour: SsdStatus.colour(kind)

    implicitWidth: 12
    implicitHeight: 12

    onKindChanged: requestPaint()
    onColourChanged: requestPaint()

    // Motion only while something is actually in progress.
    RotationAnimation on rotation {
        running: icon.kind === "busy" && icon.visible
        from: 0
        to: 360
        duration: 1200
        loops: Animation.Infinite
        onRunningChanged: if (!running) icon.rotation = 0
    }

    onPaint: {
        const ctx = getContext("2d");
        const s = Math.min(width, height);
        const c = s / 2;
        const line = Math.max(1.3, s / 8);
        ctx.reset();
        ctx.strokeStyle = colour.toString();
        ctx.fillStyle = colour.toString();
        ctx.lineWidth = line;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        function mark(top, bottom, dot) {
            ctx.strokeStyle = Colour.border_2.toString();
            ctx.fillStyle = Colour.border_2.toString();
            ctx.lineWidth = Math.max(1.2, s / 9);
            ctx.beginPath();
            ctx.moveTo(c, top);
            ctx.lineTo(c, bottom);
            ctx.stroke();
            ctx.beginPath();
            ctx.arc(c, dot, s / 16 + 0.4, 0, Math.PI * 2);
            ctx.fill();
        }

        switch (kind) {
        case "ok":
            ctx.beginPath();
            ctx.arc(c, c, s * 0.34, 0, Math.PI * 2);
            ctx.fill();
            break;
        case "safe":
            ctx.beginPath();
            ctx.arc(c, c, c - line / 2, 0, Math.PI * 2);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(s * 0.3, s * 0.52);
            ctx.lineTo(s * 0.45, s * 0.66);
            ctx.lineTo(s * 0.71, s * 0.37);
            ctx.stroke();
            break;
        case "caution":
            ctx.beginPath();
            ctx.moveTo(c, s * 0.06);
            ctx.lineTo(s * 0.97, s * 0.92);
            ctx.lineTo(s * 0.03, s * 0.92);
            ctx.closePath();
            ctx.fill();
            mark(s * 0.38, s * 0.6, s * 0.76);
            break;
        case "critical":
            ctx.beginPath();
            for (let i = 0; i < 8; ++i) {
                const a = Math.PI / 8 + i * Math.PI / 4;
                const x = c + c * Math.cos(a);
                const y = c + c * Math.sin(a);
                if (i === 0)
                    ctx.moveTo(x, y);
                else
                    ctx.lineTo(x, y);
            }
            ctx.closePath();
            ctx.fill();
            mark(s * 0.24, s * 0.56, s * 0.74);
            break;
        case "busy":
            ctx.globalAlpha = 0.35;
            ctx.beginPath();
            ctx.arc(c, c, c - line / 2, 0, Math.PI * 2);
            ctx.stroke();
            ctx.globalAlpha = 1;
            ctx.beginPath();
            ctx.arc(c, c, c - line / 2, -Math.PI / 2, 0);
            ctx.stroke();
            break;
        case "off":
            ctx.beginPath();
            ctx.arc(c, c, s * 0.34, 0, Math.PI * 2);
            ctx.stroke();
            break;
        default:
            ctx.setLineDash([1.6, 1.4]);
            ctx.beginPath();
            ctx.arc(c, c, c - line / 2, 0, Math.PI * 2);
            ctx.stroke();
        }
    }
}
