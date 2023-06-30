(defvar texpresso--window-buffer nil)

(defun texpresso--window-track (&rest r)
  (with-demoted-errors "Cannot move texpresso window: %S"
    (when (process-live-p texpresso--process)
      (let ((window (get-buffer-window texpresso--window-buffer))
            rect x y w h)
        (if (not window)
            (texpresso--send 'stay-on-top nil)
          (setq rect (window-absolute-body-pixel-edges window))
          (setq x (nth 0 rect))
          (setq y (nth 1 rect))
          ;; (when (and (frame-parameter nil 'fullscreen)
          ;;            (not (frame-parameter nil 'undecorated)))
          ;;   (setq y (- y (cdr (frame-position)))))
          (setq w (- (nth 2 rect) x))
          (setq h (- (nth 3 rect) y))
          ;; (message "rect %S %S %S %S" x y w h)
          (texpresso--send 'stay-on-top t)
          (texpresso--send 'move-window x y w h))))))

(defun texpresso-window-map ()
  (interactive)
  (setq texpresso--window-buffer (get-buffer-create "*TeXpresso window*"))
  (display-buffer texpresso--window-buffer)
  (with-current-buffer texpresso--window-buffer
    (read-only-mode t)
    (add-hook 'window-configuration-change-hook #'texpresso--window-track)
    (add-hook 'move-frame-functions #'texpresso--window-track)
    (add-function :after after-focus-change-function #'texpresso--frame-focus)
    (texpresso--window-track)))

(defun texpresso--frame-focus (&rest r)
  (if (process-live-p texpresso--process)
      (texpresso--send
       'stay-on-top
       (and texpresso--window-buffer
            (or (frame-focus-state) (frame-parameter nil 'fullscreen))
            (get-buffer-window texpresso--window-buffer)
            t))))
